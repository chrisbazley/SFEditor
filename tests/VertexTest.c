/* SFEditor test: OS-coordinate vectors and bounding boxes.
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
#include "../Vertex.h"

static BBox bbox(int xmin, int ymin, int xmax, int ymax)
{
  return (BBox){.xmin = xmin, .ymin = ymin, .xmax = xmax, .ymax = ymax};
}

static void check_vertex(Vertex actual, Vertex expected)
{
  assert(actual.x == expected.x);
  assert(actual.y == expected.y);
}

static void check_box(BBox const *actual, BBox expected)
{
  assert(actual->xmin == expected.xmin);
  assert(actual->ymin == expected.ymin);
  assert(actual->xmax == expected.xmax);
  assert(actual->ymax == expected.ymax);
}

static void test1(void)
{
  /* Component maximum */

  check_vertex(Vertex_max((Vertex){-7, 11}, (Vertex){3, -5}), (Vertex){3, 11});
  check_vertex(Vertex_max((Vertex){INT_MIN, INT_MAX}, (Vertex){INT_MAX, INT_MIN}),
               (Vertex){INT_MAX, INT_MAX});
}

static void test2(void)
{
  /* Component minimum */

  check_vertex(Vertex_min((Vertex){-7, 11}, (Vertex){3, -5}), (Vertex){-7, -5});
  check_vertex(Vertex_min((Vertex){INT_MIN, INT_MAX}, (Vertex){INT_MAX, INT_MIN}),
               (Vertex){INT_MIN, INT_MIN});
}

static void test3(void)
{
  /* Addition */

  check_vertex(Vertex_add((Vertex){-7, 11}, (Vertex){3, -5}), (Vertex){-4, 6});
  check_vertex(Vertex_add((Vertex){INT_MIN, INT_MAX - 1}, (Vertex){1, 1}),
               (Vertex){INT_MIN + 1, INT_MAX});
}

static void test4(void)
{
  /* Subtraction */

  check_vertex(Vertex_sub((Vertex){-7, 11}, (Vertex){3, -5}), (Vertex){-10, 16});
  check_vertex(Vertex_sub((Vertex){INT_MIN + 1, INT_MAX}, (Vertex){1, 1}),
               (Vertex){INT_MIN, INT_MAX - 1});
}

static void test5(void)
{
  /* Component multiplication */

  check_vertex(Vertex_mul((Vertex){-7, 11}, (Vertex){3, -5}), (Vertex){-21, -55});
  check_vertex(Vertex_mul((Vertex){-7, -11}, (Vertex){-3, -5}), (Vertex){21, 55});
  check_vertex(Vertex_mul((Vertex){INT_MIN, INT_MAX}, (Vertex){1, 0}),
               (Vertex){INT_MIN, 0});
}

static void test6(void)
{
  /* Division truncation */

  check_vertex(Vertex_div((Vertex){7, -7}, (Vertex){3, 3}), (Vertex){2, -2});
  check_vertex(Vertex_div((Vertex){7, -7}, (Vertex){-3, -3}), (Vertex){-2, 2});
  check_vertex(Vertex_div((Vertex){INT_MIN, INT_MAX}, (Vertex){1, 1}),
               (Vertex){INT_MIN, INT_MAX});
}

static void test7(void)
{
  /* Scalar power-of-two multiplication */

  check_vertex(Vertex_mul_log2((Vertex){3, 7}, 2), (Vertex){12, 28});
  check_vertex(Vertex_mul_log2((Vertex){7, 17}, -2), (Vertex){1, 4});
  check_vertex(Vertex_mul_log2((Vertex){0, 7}, 0), (Vertex){0, 7});
}

static void test8(void)
{
  /* Paired power-of-two multiplication */

  check_vertex(Vertex_mul_log2_pair((Vertex){3, 17}, (Vertex){2, -2}), (Vertex){12, 4});
  check_vertex(Vertex_mul_log2_pair((Vertex){17, 3}, (Vertex){-2, 2}), (Vertex){4, 12});
  check_vertex(Vertex_mul_log2_pair((Vertex){0, 7}, (Vertex){0, 0}), (Vertex){0, 7});
}

static void test9(void)
{
  /* Scalar power-of-two division */

  check_vertex(Vertex_div_log2((Vertex){7, 17}, 2), (Vertex){1, 4});
  check_vertex(Vertex_div_log2((Vertex){3, 7}, -2), (Vertex){12, 28});
  check_vertex(Vertex_div_log2((Vertex){0, 7}, 0), (Vertex){0, 7});
}

static void test10(void)
{
  /* Paired power-of-two division */

  check_vertex(Vertex_div_log2_pair((Vertex){17, 3}, (Vertex){2, -2}), (Vertex){4, 12});
  check_vertex(Vertex_div_log2_pair((Vertex){3, 17}, (Vertex){-2, 2}), (Vertex){12, 4});
  check_vertex(Vertex_div_log2_pair((Vertex){0, 7}, (Vertex){0, 0}), (Vertex){0, 7});
}

static void test11(void)
{
  /* Equal vertices */

  assert(Vertex_compare((Vertex){-7, 11}, (Vertex){-7, 11}));
  assert(Vertex_compare((Vertex){INT_MIN, INT_MAX}, (Vertex){INT_MIN, INT_MAX}));
}

static void test12(void)
{
  /* Unequal vertices */

  static Vertex const others[] = {{-6, 11}, {-7, 12}, {-6, 12}};
  for (size_t i = 0; i < ARRAY_SIZE(others); ++i)
  {
    assert(!Vertex_compare((Vertex){-7, 11}, others[i]));
  }
}

static void test13(void)
{
  /* Valid rectangle */

  BBox const a = bbox(-2, -3, 4, 5);
  assert(BBox_is_valid(&a));
}

static void test14(void)
{
  /* Invalid rectangle */

  BBox const reversed_x = bbox(4, -3, -2, 5);
  BBox const reversed_y = bbox(-2, 5, 4, -3);
  assert(!BBox_is_valid(&reversed_x));
  assert(!BBox_is_valid(&reversed_y));
}

static void test15(void)
{
  /* Box size */

  BBox const a = bbox(-2, -3, 4, 5);
  Vertex const size = BBox_size(&a);
  assert(size.x == 6);
  assert(size.y == 8);
}

static void test16(void)
{
  /* Empty box size */

  BBox const a = bbox(-1, -1, -1, -1);
  assert(BBox_is_valid(&a));
  Vertex const size = BBox_size(&a);
  assert(size.x == 0);
  assert(size.y == 0);
}

static void test17(void)
{
  /* Contains rectangle */

  BBox const outer = bbox(-2, -3, 4, 5), inner = bbox(-1, -2, 3, 4);
  assert(BBox_contains(&outer, &inner));
  assert(BBox_contains(&outer, &outer));
}

static void test18(void)
{
  /* Reject protruding rectangle */

  BBox const a = bbox(-2, -3, 4, 5);
  static BBox const outside[] = {
    {.xmin = -3, .ymin = -3, .xmax = 4, .ymax = 5},
    {.xmin = -2, .ymin = -4, .xmax = 4, .ymax = 5},
    {.xmin = -2, .ymin = -3, .xmax = 5, .ymax = 5},
    {.xmin = -2, .ymin = -3, .xmax = 4, .ymax = 6}
  };
  for (size_t i = 0; i < ARRAY_SIZE(outside); ++i)
  {
    assert(!BBox_contains(&a, &outside[i]));
  }
}

static void test19(void)
{
  /* Intersect rectangles */

  BBox const a = bbox(-2, -3, 4, 5), b = bbox(1, -5, 7, 2);
  BBox result;
  BBox_intersection(&a, &b, &result);
  check_box(&result, bbox(1, -3, 4, 2));
}

static void test20(void)
{
  /* Intersect in place */

  BBox a = bbox(-2, -3, 4, 5), b = bbox(1, -5, 7, 2);
  BBox_intersection(&a, &b, &a);
  check_box(&a, bbox(1, -3, 4, 2));

  a = bbox(-2, -3, 4, 5);
  BBox_intersection(&a, &b, &b);
  check_box(&b, bbox(1, -3, 4, 2));
}

static void test21(void)
{
  /* Disjoint intersection */

  BBox const a = bbox(0, 0, 2, 2);
  static BBox const outside[] = {
    {.xmin = -3, .ymin = 0, .xmax = -1, .ymax = 2},
    {.xmin = 3, .ymin = 0, .xmax = 5, .ymax = 2},
    {.xmin = 0, .ymin = -3, .xmax = 2, .ymax = -1},
    {.xmin = 0, .ymin = 3, .xmax = 2, .ymax = 5},
  };
  for (size_t i = 0; i < ARRAY_SIZE(outside); ++i)
  {
    BBox result;
    BBox_intersection(&a, &outside[i], &result);
    assert(!BBox_is_valid(&result));
  }
}

static void test22(void)
{
  /* Corner intersection */

  BBox const a = bbox(0, 0, 2, 2), b = bbox(2, 2, 4, 4);
  BBox result;
  BBox_intersection(&a, &b, &result);
  check_box(&result, bbox(2, 2, 2, 2));
}

static void test23(void)
{
  /* Expand invalid rectangle */

  BBox result = BBox_make_invalid();
  BBox_expand(&result, (Vertex){-2, 5});
  check_box(&result, bbox(-2, 5, -2, 5));
}

static void test24(void)
{
  /* Expand for rectangle */

  BBox result = bbox(0, 0, 2, 2), b = bbox(-2, 1, 1, 5);
  BBox_expand_for_area(&result, &b);
  check_box(&result, bbox(-2, 0, 2, 5));
}

static void test25(void)
{
  /* Translate in place */

  BBox result = bbox(-2, -3, 4, 5);
  BBox_translate(&result, (Vertex){-3, 7}, &result);
  check_box(&result, bbox(-5, 4, 1, 12));
}

static void test26(void)
{
  /* Construction */

  BBox box = BBox_make_invalid();
  BBox_make(&box, (Vertex){-7, -3}, (Vertex){11, 5});
  check_box(&box, bbox(-7, -3, 11, 5));
}

static void test27(void)
{
  /* Set minimum */

  BBox box = bbox(-1, -2, 11, 5);
  BBox_set_min(&box, (Vertex){-7, -3});
  check_box(&box, bbox(-7, -3, 11, 5));
}

static void test28(void)
{
  /* Set maximum */

  BBox box = bbox(-7, -3, 1, 2);
  BBox_set_max(&box, (Vertex){11, 5});
  check_box(&box, bbox(-7, -3, 11, 5));
}

static void test29(void)
{
  /* Get minimum */

  BBox const box = bbox(-7, -3, 11, 5);
  check_vertex(BBox_get_min(&box), (Vertex){-7, -3});
}

static void test30(void)
{
  /* Get maximum */

  BBox const box = bbox(-7, -3, 11, 5);
  check_vertex(BBox_get_max(&box), (Vertex){11, 5});
}

static void test31(void)
{
  /* Width */

  BBox const box = bbox(-7, -3, 11, 5);
  assert(BBox_width(&box) == 18);
}

static void test32(void)
{
  /* Height */

  BBox const box = bbox(-7, -3, 11, 5);
  assert(BBox_height(&box) == 8);
}

static void test33(void)
{
  /* Outlying point expansion */

  BBox box = bbox(-7, -3, 11, 5);
  BBox_expand(&box, (Vertex){-10, 9});
  check_box(&box, bbox(-10, -3, 11, 9));

  box = bbox(-7, -3, 11, 5);
  BBox_expand(&box, (Vertex){15, -6});
  check_box(&box, bbox(-7, -6, 15, 5));
}

static void test34(void)
{
  /* Contained point expansion */

  BBox const original = bbox(-7, -3, 11, 5);
  BBox box = original;
  BBox_expand(&box, (Vertex){0, 0});
  check_box(&box, original);
  BBox_expand(&box, (Vertex){-7, 5});
  check_box(&box, original);
}

static void test35(void)
{
  /* First area expansion */

  BBox box = BBox_make_invalid();
  BBox const area = bbox(-7, -3, 11, 5);
  BBox_expand_for_area(&box, &area);
  check_box(&box, area);
}

static void test36(void)
{
  /* Edge intersection */

  BBox const a = bbox(0, 0, 2, 2), b = bbox(2, 0, 4, 2);
  BBox result;
  BBox_intersection(&a, &b, &result);
  assert(BBox_is_valid(&result));
  check_box(&result, bbox(2, 0, 2, 2));
  assert(BBox_width(&result) == 0);
}

void Vertex_tests(void)
{
  /* Vertex */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Component maximum", test1 },
    { "Component minimum", test2 },
    { "Addition", test3 },
    { "Subtraction", test4 },
    { "Component multiplication", test5 },
    { "Division truncation", test6 },
    { "Scalar power-of-two multiplication", test7 },
    { "Paired power-of-two multiplication", test8 },
    { "Scalar power-of-two division", test9 },
    { "Paired power-of-two division", test10 },
    { "Equal vertices", test11 },
    { "Unequal vertices", test12 },
    { "Valid rectangle", test13 },
    { "Invalid rectangle", test14 },
    { "Box size", test15 },
    { "Empty box size", test16 },
    { "Contains rectangle", test17 },
    { "Reject protruding rectangle", test18 },
    { "Intersect rectangles", test19 },
    { "Intersect in place", test20 },
    { "Disjoint intersection", test21 },
    { "Corner intersection", test22 },
    { "Expand invalid rectangle", test23 },
    { "Expand for rectangle", test24 },
    { "Translate in place", test25 },
    { "Construction", test26 },
    { "Set minimum", test27 },
    { "Set maximum", test28 },
    { "Get minimum", test29 },
    { "Get maximum", test30 },
    { "Width", test31 },
    { "Height", test32 },
    { "Outlying point expansion", test33 },
    { "Contained point expansion", test34 },
    { "First area expansion", test35 },
    { "Edge intersection", test36 },
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
