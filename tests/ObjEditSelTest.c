/* SFEditor: object-selection model tests.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../ObjEditSel.h"

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
  MapPoint last;
} Redraw;

typedef struct {
  size_t count;
  MapPoint points[16];
} Collected;

static void redraw(MapPoint p, void *arg)
{
  Redraw *const log = arg;
  *log = (Redraw){.calls = log->calls + 1, .last = p};
}

static void collect(MapPoint p, void *arg)
{
  Collected *const result = arg;
  assert(result->count < ARRAY_SIZE(result->points));
  result->points[result->count++] = point(
    (p.x % Obj_Size + Obj_Size) % Obj_Size,
    (p.y % Obj_Size + Obj_Size) % Obj_Size);
}

static void check_collected(Collected const *result,
  _Optional MapPoint const *expected, size_t count)
{
  assert(result->count == count);
  bool seen[16] = {false};
  assert(count <= ARRAY_SIZE(seen));
  for (size_t j = 0; j < result->count; ++j)
  {
    assert(expected);
    size_t i = 0;
    while (i < count && !MapPoint_compare(result->points[j], expected[i]))
    {
      ++i;
    }
    assert(i < count);
    assert(!seen[i]);
    seen[i] = true;
  }
}

static void check_selection(ObjEditSelection *selection,
  _Optional MapPoint const *expected, size_t count)
{
  assert(ObjEditSelection_size(selection) == count);
  assert(ObjEditSelection_is_none(selection) == (count == 0));
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      bool want = false;
      for (size_t i = 0; i < count; ++i)
      {
        assert(expected);
        want |= expected[i].x == x && expected[i].y == y;
      }
      assert(ObjEditSelection_is_selected(selection, point(x, y)) == want);
    }
  }

}

static void select_points(ObjEditSelection *selection,
  MapPoint const points[], size_t count)
{
  for (size_t i = 0; i < count; ++i)
  {
    ObjEditSelection_select(selection, points[i]);
  }
}

static MapPoint const points[] = {
  {7, 0}, {8, 0}, {31, 0}, {32, 0}, {Obj_Size - 1, Obj_Size - 1}
};

static MapPoint const wrapped[] = {
  {Obj_Size - 1, Obj_Size - 1}, {0, Obj_Size - 1},
  {Obj_Size - 1, 0}, {0, 0}
};

static void check_iterator(ObjEditSelection *selection,
  _Optional MapPoint const *expected, size_t count)
{
  Collected result = {0};
  ObjEditSelIter iter;
  for (MapPoint p = ObjEditSelIter_get_first(&iter, selection);
       !ObjEditSelIter_done(&iter); p = ObjEditSelIter_get_next(&iter))
  {
    collect(p, &result);
  }
  check_collected(&result, expected, count);
}

static void test1(void)
{
  /* Initialise */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  check_selection(&selection, NULL, 0);
  MapArea bounds;
  assert(!ObjEditSelection_get_bounds(&selection, &bounds));
  ObjEditSelection_destroy(&selection);
}

static void test2(void)
{
  /* Select points */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  check_selection(&selection, points, ARRAY_SIZE(points));
  ObjEditSelection_destroy(&selection);
}

static void test3(void)
{
  /* Select twice */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  ObjEditSelection_select(&selection, points[0]);
  ObjEditSelection_select(&selection, points[0]);
  check_selection(&selection, points, 1);
  assert(log.calls == 1);
  ObjEditSelection_destroy(&selection);
}

static void test4(void)
{
  /* Deselect */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, 2);
  ObjEditSelection_deselect(&selection, points[1]);
  check_selection(&selection, points, 1);
  ObjEditSelection_destroy(&selection);
}

static void test5(void)
{
  /* Invert */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  ObjEditSelection_invert(&selection, points[0], true);
  check_selection(&selection, points, 1);
  ObjEditSelection_invert(&selection, points[0], true);
  check_selection(&selection, NULL, 0);
  ObjEditSelection_destroy(&selection);
}

static void test6(void)
{
  /* Clear */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  ObjEditSelection_clear(&selection);
  check_selection(&selection, NULL, 0);
  ObjEditSelection_destroy(&selection);
}

static void test7(void)
{
  /* Wrap point */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  ObjEditSelection_select(&selection, point(-1, -1));
  assert(ObjEditSelection_is_selected(&selection, point(Obj_Size - 1, Obj_Size - 1)));
  assert(ObjEditSelection_is_selected(&selection, point(Obj_Size * 2 - 1, Obj_Size * 2 - 1)));
  ObjEditSelection_destroy(&selection);
}

static void test8(void)
{
  /* Wrap area */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(-1, -1, 0, 0);
  ObjEditSelection_select_area(&selection, &a);
  check_selection(&selection, wrapped, ARRAY_SIZE(wrapped));
  ObjEditSelection_destroy(&selection);
}

static void test9(void)
{
  /* Empty iterator */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  check_iterator(&selection, NULL, 0);
  ObjEditSelection_destroy(&selection);
}

static void test10(void)
{
  /* Iterate */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  check_iterator(&selection, points, ARRAY_SIZE(points));
  ObjEditSelection_destroy(&selection);
}

static void test11(void)
{
  /* Iterate across wrap */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(-1, -1, 0, 0);
  ObjEditSelection_select_area(&selection, &a);
  ObjEditSelection_select(&selection, point(Obj_Size + 1, 0));
  static MapPoint const expected[] = {
    {Obj_Size - 1, Obj_Size - 1}, {0, Obj_Size - 1},
    {Obj_Size - 1, 0}, {0, 0}, {1, 0}
  };
  check_iterator(&selection, expected, ARRAY_SIZE(expected));
  ObjEditSelection_destroy(&selection);
}

static void test12(void)
{
  /* Selection bounds */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  MapArea bounds;
  assert(ObjEditSelection_get_bounds(&selection, &bounds));
  check_bounds(&bounds, area(7, 0, Obj_Size - 1, Obj_Size - 1));
  ObjEditSelection_destroy(&selection);
}

static void test13(void)
{
  /* Bounds after deselection */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  ObjEditSelection_deselect(&selection, points[ARRAY_SIZE(points) - 1]);
  MapArea bounds;
  assert(ObjEditSelection_get_bounds(&selection, &bounds));
  check_bounds(&bounds, area(7, 0, 32, 0));
  ObjEditSelection_destroy(&selection);
}

static void test14(void)
{
  /* Selection redraw */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  ObjEditSelection_select(&selection, points[0]);
  assert(log.calls == 1);
  assert(MapPoint_compare(log.last, points[0]));
  ObjEditSelection_destroy(&selection);
}

static void test15(void)
{
  /* Deselection redraw */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  ObjEditSelection_select(&selection, point(-1, -1));
  ObjEditSelection_deselect(&selection, point(-1, -1));
  assert(log.calls == 2);
  assert(MapPoint_compare(log.last, point(-1, -1)));
  ObjEditSelection_destroy(&selection);
}

static void test16(void)
{
  /* Clear redraw */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, 2);
  ObjEditSelection_clear(&selection);
  assert(log.calls == 4);
  ObjEditSelection_destroy(&selection);
}

static void test17(void)
{
  /* Deselect area */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(-1, -1, 0, 0);
  ObjEditSelection_select_area(&selection, &a);
  ObjEditSelection_deselect_area(&selection, &a);
  check_selection(&selection, NULL, 0);
  ObjEditSelection_destroy(&selection);
}

static void test18(void)
{
  /* Select area twice */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const a = area(-1, -1, 0, 0);
  ObjEditSelection_select_area(&selection, &a);
  ObjEditSelection_select_area(&selection, &a);
  check_selection(&selection, wrapped, ARRAY_SIZE(wrapped));
  assert(log.calls == ARRAY_SIZE(wrapped));
  ObjEditSelection_destroy(&selection);
}

static void test19(void)
{
  /* Suppress inversion redraw */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  ObjEditSelection_invert(&selection, points[0], false);
  assert(log.calls == 0);
  ObjEditSelection_destroy(&selection);
}

static void test20(void)
{
  /* Inversion redraw */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  ObjEditSelection_invert(&selection, points[0], true);
  assert(log.calls == 1);
  assert(MapPoint_compare(log.last, points[0]));
  ObjEditSelection_destroy(&selection);
}

static void test21(void)
{
  /* Select all */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const whole = area(0, 0, Obj_Size - 1, Obj_Size - 1);
  ObjEditSelection_select_area(&selection, &whole);
  assert(ObjEditSelection_is_all(&selection));
  assert(ObjEditSelection_size(&selection) == Obj_Area);
  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      assert(ObjEditSelection_is_selected(&selection, point(x, y)));
    }
  }
  MapArea bounds;
  assert(ObjEditSelection_get_bounds(&selection, &bounds));
  check_bounds(&bounds, area(0, 0, Obj_Size - 1, Obj_Size - 1));
  ObjEditSelection_destroy(&selection);
}

static void test22(void)
{
  /* Select all twice */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const whole = area(0, 0, Obj_Size - 1, Obj_Size - 1);
  ObjEditSelection_select_area(&selection, &whole);
  ObjEditSelection_select_area(&selection, &whole);
  assert(log.calls == Obj_Area);
  ObjEditSelection_destroy(&selection);
}

static void test23(void)
{
  /* Deselect from full selection */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const whole = area(0, 0, Obj_Size - 1, Obj_Size - 1);
  ObjEditSelection_select_area(&selection, &whole);
  ObjEditSelection_deselect(&selection, point(0, 0));
  assert(!ObjEditSelection_is_all(&selection));
  assert(ObjEditSelection_size(&selection) == Obj_Area - 1);
  ObjEditSelection_destroy(&selection);
}

static void test24(void)
{
  /* Clear full selection */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  MapArea const whole = area(0, 0, Obj_Size - 1, Obj_Size - 1);
  ObjEditSelection_select_area(&selection, &whole);
  ObjEditSelection_clear(&selection);
  check_selection(&selection, NULL, 0);
  ObjEditSelection_destroy(&selection);
}

static void test25(void)
{
  /* Visit selection */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  select_points(&selection, points, ARRAY_SIZE(points));
  Collected result = {0};
  assert(ObjEditSelection_for_each(&selection, collect, &result));
  check_collected(&result, points, ARRAY_SIZE(points));
  ObjEditSelection_destroy(&selection);
}

static void test26(void)
{
  /* Visit empty selection */
  Redraw log = {0};
  ObjEditSelection selection;
  assert(!SFError_fail(ObjEditSelection_init(&selection, redraw, &log)));
  Collected result = {0};
  assert(!ObjEditSelection_for_each(&selection, collect, &result));
  assert(result.count == 0);
  ObjEditSelection_destroy(&selection);
}

static void test27(void)
{
  /* Copy selection */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  select_points(&a, points, 2);
  ObjEditSelection_select(&b, point(9, 9));
  ObjEditSelection_copy(&b, &a);
  check_selection(&b, points, 2);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test28(void)
{
  /* Copy independence */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  select_points(&a, points, 2);
  ObjEditSelection_select(&b, point(9, 9));
  ObjEditSelection_copy(&b, &a);
  ObjEditSelection_deselect(&b, points[0]);
  check_selection(&a, points, 2);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test29(void)
{
  /* Copy callback */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  select_points(&a, points, 2);
  ObjEditSelection_select(&b, point(9, 9));
  ObjEditSelection_copy(&b, &a);
  size_t const a_calls = a_log.calls, b_calls = b_log.calls;
  ObjEditSelection_select(&b, point(3, 3));
  assert(a_log.calls == a_calls);
  assert(b_log.calls == b_calls + 1);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test30(void)
{
  /* Copy empty selection */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  ObjEditSelection_select(&b, points[0]);
  ObjEditSelection_copy(&b, &a);
  check_selection(&b, NULL, 0);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test31(void)
{
  /* Enumerate changes */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  ObjEditSelection_select(&a, point(1, 1));
  ObjEditSelection_select(&a, point(2, 2));
  ObjEditSelection_select(&b, point(2, 2));
  ObjEditSelection_select(&b, point(3, 3));
  Collected changes = {0};
  assert(ObjEditSelection_for_each_changed(&a, &b, NULL, collect, &changes));
  static MapPoint const expected[] = {{1, 1}, {3, 3}};
  check_collected(&changes, expected, ARRAY_SIZE(expected));
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test32(void)
{
  /* Clip changes */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  ObjEditSelection_select(&a, point(1, 1));
  ObjEditSelection_select(&a, point(2, 2));
  ObjEditSelection_select(&b, point(2, 2));
  ObjEditSelection_select(&b, point(3, 3));
  Collected changes = {0};
  MapArea const clip = area(2, 2, 3, 3);
  assert(ObjEditSelection_for_each_changed(&a, &b, &clip, collect, &changes));
  MapPoint const expected = {3, 3};
  check_collected(&changes, &expected, 1);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test33(void)
{
  /* Clip unchanged point */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  ObjEditSelection_select(&a, point(1, 1));
  ObjEditSelection_select(&a, point(2, 2));
  ObjEditSelection_select(&b, point(2, 2));
  ObjEditSelection_select(&b, point(3, 3));
  Collected changes = {0};
  MapArea const clip = area(2, 2, 2, 2);
  assert(!ObjEditSelection_for_each_changed(&a, &b, &clip, collect, &changes));
  assert(changes.count == 0);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test34(void)
{
  /* Disjoint clipping rectangle */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  ObjEditSelection_select(&a, point(1, 1));
  ObjEditSelection_select(&a, point(2, 2));
  ObjEditSelection_select(&b, point(2, 2));
  ObjEditSelection_select(&b, point(3, 3));
  Collected changes = {0};
  MapArea const clip = area(20, 20, 21, 21);
  assert(!ObjEditSelection_for_each_changed(&a, &b, &clip, collect, &changes));
  assert(changes.count == 0);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test35(void)
{
  /* Unchanged selections */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  select_points(&a, points, 2);
  ObjEditSelection_select(&b, point(9, 9));
  ObjEditSelection_copy(&b, &a);
  Collected changes = {0};
  assert(!ObjEditSelection_for_each_changed(&a, &b, NULL, collect, &changes));
  assert(changes.count == 0);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test36(void)
{
  /* Empty selection changes */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  Collected changes = {0};
  assert(!ObjEditSelection_for_each_changed(&a, &b, NULL, collect, &changes));
  assert(changes.count == 0);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

static void test37(void)
{
  /* Changes from empty selection */
  Redraw a_log = {0}, b_log = {0};
  ObjEditSelection a, b;
  assert(!SFError_fail(ObjEditSelection_init(&a, redraw, &a_log)));
  assert(!SFError_fail(ObjEditSelection_init(&b, redraw, &b_log)));
  select_points(&b, points, 2);
  Collected changes = {0};
  assert(ObjEditSelection_for_each_changed(&a, &b, NULL, collect, &changes));
  check_collected(&changes, points, 2);
  ObjEditSelection_destroy(&a);
  ObjEditSelection_destroy(&b);
}

void ObjEditSel_tests(void)
{
  /* ObjEditSel */
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
    { "Deselect area", test17 },
    { "Select area twice", test18 },
    { "Suppress inversion redraw", test19 },
    { "Inversion redraw", test20 },
    { "Select all", test21 },
    { "Select all twice", test22 },
    { "Deselect from full selection", test23 },
    { "Clear full selection", test24 },
    { "Visit selection", test25 },
    { "Visit empty selection", test26 },
    { "Copy selection", test27 },
    { "Copy independence", test28 },
    { "Copy callback", test29 },
    { "Copy empty selection", test30 },
    { "Enumerate changes", test31 },
    { "Clip changes", test32 },
    { "Clip unchanged point", test33 },
    { "Disjoint clipping rectangle", test34 },
    { "Unchanged selections", test35 },
    { "Empty selection changes", test36 },
    { "Changes from empty selection", test37 },
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
