/* SFEditor: tile-selection model tests.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../MapEditSel.h"

static MapPoint point(MapCoord x, MapCoord y)
{
  return (MapPoint){x, y};
}

static MapArea area(MapCoord x0, MapCoord y0, MapCoord x1, MapCoord y1)
{
  return (MapArea){
    .min = {x0, y0},
    .max = {x1, y1}
  };
}

static void check_bounds(MapArea const *bounds, MapArea expected)
{
  assert(bounds->min.x == expected.min.x);
  assert(bounds->min.y == expected.min.y);
  assert(bounds->max.x == expected.max.x);
  assert(bounds->max.y == expected.max.y);
}

typedef struct {
  size_t calls;
  MapArea last;
} Redraw;

static void redraw(MapArea const *bounds, void *arg)
{
  Redraw *const log = arg;
  assert(MapArea_is_valid(bounds));
  *log = (Redraw){.calls = log->calls + 1, .last = *bounds};
}

/* Compare every cell with an explicit expected set, then check iteration
 * independently for omissions and duplicates, including wrapped positions. */
static void check_selection(MapEditSelection *selection,
  _Optional MapPoint const *expected, size_t count)
{
  assert(MapEditSelection_size(selection) == count);
  assert(MapEditSelection_is_none(selection) == (count == 0));

  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      bool want = false;
      for (size_t i = 0; i < count; ++i)
      {
        assert(expected);
        want |= expected[i].x == x && expected[i].y == y;
      }
      assert(MapEditSelection_is_selected(selection, point(x, y)) == want);
    }
  }

}

static void select_points(MapEditSelection *selection,
  MapPoint const points[], size_t count)
{
  for (size_t i = 0; i < count; ++i)
  {
    MapEditSelection_select(selection, points[i]);
  }
}

static MapPoint const points[] = {
  {7, 0}, {8, 0}, {31, 0}, {32, 0}, {Map_Size - 1, Map_Size - 1}
};

static MapPoint const wrapped[] = {
  {Map_Size - 1, Map_Size - 1}, {0, Map_Size - 1},
  {Map_Size - 1, 0}, {0, 0}
};

static void check_iterator(MapEditSelection *selection,
  _Optional MapPoint const *expected, size_t count)
{
  bool seen[16] = {false};
  assert(count <= ARRAY_SIZE(seen));
  size_t visited = 0;
  MapEditSelIter iter;
  for (MapPoint p = MapEditSelIter_get_first(&iter, selection);
       !MapEditSelIter_done(&iter); p = MapEditSelIter_get_next(&iter))
  {
    assert(visited++ < count);
    p = point((p.x % Map_Size + Map_Size) % Map_Size,
              (p.y % Map_Size + Map_Size) % Map_Size);
    assert(expected);
    size_t i = 0;
    while (i < count && !MapPoint_compare(p, expected[i]))
    {
      ++i;
    }
    assert(i < count);
    assert(!seen[i]);
    seen[i] = true;
  }
  assert(visited == count);
}

static void test1(void)
{
  /* Initialise */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  check_selection(&selection, NULL, 0);
  MapArea bounds;
  assert(!MapEditSelection_get_bounds(&selection, &bounds));
  MapEditSelection_destroy(&selection);
}

static void test2(void)
{
  /* Select points */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  check_selection(&selection, points, ARRAY_SIZE(points));
  MapEditSelection_destroy(&selection);
}

static void test3(void)
{
  /* Select twice */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select(&selection, points[0]);
  MapEditSelection_select(&selection, points[0]);
  check_selection(&selection, points, 1);
  assert(log.calls == 1);
  MapEditSelection_destroy(&selection);
}

static void test4(void)
{
  /* Deselect */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, 2);
  MapEditSelection_deselect(&selection, points[1]);
  check_selection(&selection, points, 1);
  MapEditSelection_destroy(&selection);
}

static void test5(void)
{
  /* Invert */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_invert(&selection, points[0]);
  check_selection(&selection, points, 1);
  MapEditSelection_invert(&selection, points[0]);
  check_selection(&selection, NULL, 0);
  MapEditSelection_destroy(&selection);
}

static void test6(void)
{
  /* Clear */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  MapEditSelection_clear(&selection);
  check_selection(&selection, NULL, 0);
  MapEditSelection_destroy(&selection);
}

static void test7(void)
{
  /* Wrap point */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select(&selection, point(-1, -1));
  assert(MapEditSelection_is_selected(&selection, point(Map_Size - 1, Map_Size - 1)));
  assert(MapEditSelection_is_selected(&selection, point(Map_Size * 2 - 1, Map_Size * 2 - 1)));
  MapEditSelection_destroy(&selection);
}

static void test8(void)
{
  /* Wrap area */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(-1, -1, 0, 0);
  MapEditSelection_select_area(&selection, &a);
  check_selection(&selection, wrapped, ARRAY_SIZE(wrapped));
  MapEditSelection_destroy(&selection);
}

static void test9(void)
{
  /* Empty iterator */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  check_iterator(&selection, NULL, 0);
  MapEditSelection_destroy(&selection);
}

static void test10(void)
{
  /* Iterate */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  check_iterator(&selection, points, ARRAY_SIZE(points));
  MapEditSelection_destroy(&selection);
}

static void test11(void)
{
  /* Iterate across wrap */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(-1, -1, 0, 0);
  MapEditSelection_select_area(&selection, &a);
  MapEditSelection_select(&selection, point(Map_Size + 1, 0));
  static MapPoint const expected[] = {
    {Map_Size - 1, Map_Size - 1}, {0, Map_Size - 1},
    {Map_Size - 1, 0}, {0, 0}, {1, 0}
  };
  check_iterator(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test12(void)
{
  /* Selection bounds */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  MapArea bounds;
  assert(MapEditSelection_get_bounds(&selection, &bounds));
  check_bounds(&bounds, area(7, 0, Map_Size - 1, Map_Size - 1));
  MapEditSelection_destroy(&selection);
}

static void test13(void)
{
  /* Bounds after deselection */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  MapEditSelection_deselect(&selection, points[ARRAY_SIZE(points) - 1]);
  MapArea bounds;
  assert(MapEditSelection_get_bounds(&selection, &bounds));
  check_bounds(&bounds, area(7, 0, 32, 0));
  MapEditSelection_destroy(&selection);
}

static void test14(void)
{
  /* Selection redraw */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select(&selection, points[0]);
  assert(log.calls == 1);
  check_bounds(&log.last, area(7, 0, 7, 0));
  MapEditSelection_destroy(&selection);
}

static void test15(void)
{
  /* Deselection redraw */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select(&selection, point(-1, -1));
  MapEditSelection_deselect(&selection, point(-1, -1));
  assert(log.calls == 2);
  check_bounds(&log.last, area(-1, -1, -1, -1));
  MapEditSelection_destroy(&selection);
}

static void test16(void)
{
  /* Clear redraw */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, 2);
  MapEditSelection_clear(&selection);
  assert(log.calls == 3);
  MapEditSelection_destroy(&selection);
}

static void test17(void)
{
  /* Select overlapping areas */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(1, 1, 2, 2);
  MapEditSelection_select_area(&selection, &a);
  MapArea const b = area(2, 1, 3, 2);
  MapEditSelection_select_area(&selection, &b);
  static MapPoint const expected[] = {
    {1, 1}, {2, 1}, {3, 1}, {1, 2}, {2, 2}, {3, 2}
  };
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test18(void)
{
  /* Deselect area */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(1, 1, 2, 2);
  MapEditSelection_select_area(&selection, &a);
  MapArea const b = area(2, 1, 2, 2);
  MapEditSelection_deselect_area(&selection, &b);
  static MapPoint const expected[] = {{1, 1}, {1, 2}};
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test19(void)
{
  /* Deselect area twice */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(1, 1, 2, 2);
  MapEditSelection_select_area(&selection, &a);
  MapEditSelection_deselect_area(&selection, &a);
  MapEditSelection_deselect_area(&selection, &a);
  assert(log.calls == 2);
  check_selection(&selection, NULL, 0);
  MapEditSelection_destroy(&selection);
}

static void test20(void)
{
  /* Invert area */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(1, 1, 2, 2);
  MapEditSelection_select(&selection, point(1, 1));
  MapEditSelection_select(&selection, point(1, 2));
  MapEditSelection_invert_area(&selection, &a, true);
  static MapPoint const expected[] = {{2, 1}, {2, 2}};
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test21(void)
{
  /* Suppress area redraw */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(1, 1, 2, 2);
  MapEditSelection_select(&selection, point(1, 1));
  MapEditSelection_select(&selection, point(1, 2));
  MapEditSelection_invert_area(&selection, &a, false);
  assert(log.calls == 2);
  MapEditSelection_destroy(&selection);
}

static void test22(void)
{
  /* Area inversion redraw */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(1, 1, 2, 2);
  MapEditSelection_select(&selection, point(1, 1));
  MapEditSelection_select(&selection, point(1, 2));
  MapEditSelection_invert_area(&selection, &a, true);
  assert(log.calls == 3);
  check_bounds(&log.last, a);
  MapEditSelection_destroy(&selection);
}

static void test23(void)
{
  /* Select all */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_all(&selection);
  assert(MapEditSelection_is_all(&selection));
  assert(MapEditSelection_size(&selection) == Map_Area);
  for (MapCoord y = 0; y < Map_Size; ++y)
  {
    for (MapCoord x = 0; x < Map_Size; ++x)
    {
      assert(MapEditSelection_is_selected(&selection, point(x, y)));
    }
  }
  MapArea bounds;
  assert(MapEditSelection_get_bounds(&selection, &bounds));
  check_bounds(&bounds, area(0, 0, Map_Size - 1, Map_Size - 1));
  MapEditSelection_destroy(&selection);
}

static void test24(void)
{
  /* Select all twice */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_all(&selection);
  MapEditSelection_select_all(&selection);
  assert(log.calls == 1);
  MapEditSelection_destroy(&selection);
}

static void test25(void)
{
  /* Deselect from full selection */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_all(&selection);
  MapEditSelection_deselect(&selection, point(0, 0));
  assert(!MapEditSelection_is_all(&selection));
  assert(MapEditSelection_size(&selection) == Map_Area - 1);
  MapEditSelection_destroy(&selection);
}

static void test26(void)
{
  /* Clear full selection */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_all(&selection);
  MapEditSelection_clear(&selection);
  check_selection(&selection, NULL, 0);
  MapEditSelection_destroy(&selection);
}

static void test27(void)
{
  /* Select rectangle */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_rect(&selection, point(2, 2), point(1, 1));
  static MapPoint const expected[] = {{1, 1}, {2, 1}, {1, 2}, {2, 2}};
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test28(void)
{
  /* Select line */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_line(&selection, point(3, 2), point(1, 2), 0);
  static MapPoint const expected[] = {{1, 2}, {2, 2}, {3, 2}};
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test29(void)
{
  /* Select zero-radius circle */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_circ(&selection, point(10, 10), 0);
  static MapPoint const expected[] = {{10, 10}};
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test30(void)
{
  /* Select circle */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_circ(&selection, point(10, 10), 1);
  static MapPoint const expected[] = {{10, 10}, {9, 10}, {11, 10}, {10, 9}, {10, 11}};
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test31(void)
{
  /* Select triangle */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_tri(&selection, point(1, 1), point(3, 1), point(1, 3));
  static MapPoint const expected[] = {{1, 1}, {2, 1}, {3, 1}, {1, 2}, {2, 2}, {1, 3}};
  check_selection(&selection, expected, ARRAY_SIZE(expected));
  MapEditSelection_destroy(&selection);
}

static void test32(void)
{
  /* Invert rectangle */
  Redraw log = {0};
  MapEditSelection selection;
  assert(!SFError_fail(MapEditSelection_init(&selection, redraw, &log)));
  MapEditSelection_select_rect(&selection, point(2, 2), point(1, 1));
  MapEditSelection_invert_rect(&selection, point(1, 1), point(2, 2), false);
  check_selection(&selection, NULL, 0);
  MapEditSelection_destroy(&selection);
}

void MapEditSel_tests(void)
{
  /* MapEditSel */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initialise", test1 },
    { "Select points", test2 },
    { "Select twice", test3 },
    { "Deselect", test4 },
    { "Invert", test5 },
    { "Clear", test6 },
    { "Wrap point", test7 },
    { "Wrap area", test8 },
    { "Empty iterator", test9 },
    { "Iterate", test10 },
    { "Iterate across wrap", test11 },
    { "Selection bounds", test12 },
    { "Bounds after deselection", test13 },
    { "Selection redraw", test14 },
    { "Deselection redraw", test15 },
    { "Clear redraw", test16 },
    { "Select overlapping areas", test17 },
    { "Deselect area", test18 },
    { "Deselect area twice", test19 },
    { "Invert area", test20 },
    { "Suppress area redraw", test21 },
    { "Area inversion redraw", test22 },
    { "Select all", test23 },
    { "Select all twice", test24 },
    { "Deselect from full selection", test25 },
    { "Clear full selection", test26 },
    { "Select rectangle", test27 },
    { "Select line", test28 },
    { "Select zero-radius circle", test29 },
    { "Select circle", test30 },
    { "Select triangle", test31 },
    { "Invert rectangle", test32 },
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); ++count)
  {
    DEBUGF("Test %zu/%zu : %s\n",
           1 + count,
           ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);

    Fortify_EnterScope();
    unit_tests[count].test_func();
    Fortify_LeaveScope();
  }
}
