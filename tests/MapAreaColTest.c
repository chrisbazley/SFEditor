/* SFEditor test: map area collection.
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
#include "../MapAreaCol.h"
#include "../MapAreaColData.h"

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

static size_t collect(MapAreaColData const *collection, MapArea output[MapAreaColMax])
{
  MapAreaColIter iter;
  size_t count = 0;
  for (_Optional MapArea const *p = MapAreaColIter_get_first(&iter, collection);
       p; p = MapAreaColIter_get_next(&iter))
  {
    assert(count < MapAreaColMax);
    assert(MapArea_is_valid(&*p));
    output[count++] = *p;
  }
  return count;
}

static void check_coverage(MapAreaColData const *collection,
  MapArea const inputs[], size_t input_count)
{
  MapArea output[MapAreaColMax];
  size_t const count = collect(collection, output);
  assert(count > 0);
  for (size_t k = 0; k < input_count; ++k)
  {
    bool covered = false;
    for (size_t i = 0; i < count; ++i)
    {
      if (MapArea_contains_area(&output[i], &inputs[k]))
      {
        covered = true;
      }
    }
    assert(covered);
  }
}

static void check_single(MapAreaColData const *collection, MapArea expected)
{
  MapArea output[MapAreaColMax];
  assert(collect(collection, output) == 1);
  check_area(&output[0], expected);
}

static void test1(void)
{
  /* Initialise */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapAreaColIter iter;
  assert(!MapAreaColIter_get_first(&iter, &collection));
}

static void test2(void)
{
  /* Add rectangle */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const a = area(-2, -3, 4, 5);
  MapAreaCol_add(&collection, &a);
  check_single(&collection, a);
}

static void test3(void)
{
  /* Duplicate rectangle */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const a = area(-2, -3, 4, 5);
  MapAreaCol_add(&collection, &a);
  MapAreaCol_add(&collection, &a);
  check_single(&collection, a);
}

static void test4(void)
{
  /* Contained rectangle */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const outer = area(-2, -3, 4, 5), inner = area(-1, -2, 3, 4);
  MapAreaCol_add(&collection, &outer);
  MapAreaCol_add(&collection, &inner);
  check_single(&collection, outer);
}

static void test5(void)
{
  /* Containing rectangle */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const outer = area(-2, -3, 4, 5), inner = area(-1, -2, 3, 4);
  MapAreaCol_add(&collection, &inner);
  MapAreaCol_add(&collection, &outer);
  check_single(&collection, outer);
}

static void test6(void)
{
  /* Merge overlap */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const a = area(-2, -3, 2, 2), b = area(1, 1, 4, 5);
  MapAreaCol_add(&collection, &a);
  MapAreaCol_add(&collection, &b);
  check_single(&collection, area(-2, -3, 4, 5));
}

static void test7(void)
{
  /* Merge touching edges */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const a = area(0, 0, 2, 2), b = area(2, 0, 4, 2);
  MapAreaCol_add(&collection, &a);
  MapAreaCol_add(&collection, &b);
  check_single(&collection, area(0, 0, 4, 2));
}

static void test8(void)
{
  /* Merge touching corners */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const a = area(0, 0, 2, 2), b = area(2, 2, 4, 4);
  MapAreaCol_add(&collection, &a);
  MapAreaCol_add(&collection, &b);
  check_single(&collection, area(0, 0, 4, 4));
}

static void test9(void)
{
  /* Keep disjoint rectangles */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const inputs[] = {
    {.min = {0, 0}, .max = {2, 2}}, {.min = {3, 0}, .max = {5, 2}}
  };
  MapAreaCol_add(&collection, &inputs[0]);
  MapAreaCol_add(&collection, &inputs[1]);
  MapArea output[MapAreaColMax];
  assert(collect(&collection, output) == 2);
  check_coverage(&collection, inputs, ARRAY_SIZE(inputs));
}

static void test10(void)
{
  /* Bridge rectangles */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  static MapArea const inputs[] = {
    {.min = {0, 0}, .max = {1, 1}},
    {.min = {4, 0}, .max = {5, 1}},
    {.min = {1, 0}, .max = {4, 1}},
  };
  for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i)
  {
    MapAreaCol_add(&collection, &inputs[i]);
  }
  check_single(&collection, area(0, 0, 5, 1));
}

static void test11(void)
{
  /* Cascade bounding merges */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  static MapArea const inputs[] = {
    {.min = {0, 0}, .max = {0, 4}},
    {.min = {3, 3}, .max = {4, 4}},
    {.min = {0, 0}, .max = {4, 0}},
  };
  for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i)
  {
    MapAreaCol_add(&collection, &inputs[i]);
  }
  check_single(&collection, area(0, 0, 4, 4));
}

static void test12(void)
{
  /* Fill capacity */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea inputs[MapAreaColMax];
  for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i)
  {
    MapCoord const x = (MapCoord)i * 10;
    inputs[i] = area(x, 0, x, 0);
    MapAreaCol_add(&collection, &inputs[i]);
  }
  MapArea output[MapAreaColMax];
  assert(collect(&collection, output) == MapAreaColMax);
  check_coverage(&collection, inputs, ARRAY_SIZE(inputs));
}

static void test13(void)
{
  /* Extend nearest rectangle */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea inputs[MapAreaColMax + 1];
  for (size_t i = 0; i < MapAreaColMax; ++i)
  {
    MapCoord const x = (MapCoord)i * 10;
    inputs[i] = area(x, 0, x, 0);
    MapAreaCol_add(&collection, &inputs[i]);
  }
  inputs[MapAreaColMax] = area(71, 0, 71, 0);
  MapAreaCol_add(&collection, &inputs[MapAreaColMax]);
  MapArea output[MapAreaColMax];
  size_t const count = collect(&collection, output);
  assert(count == MapAreaColMax);
  bool found = false;
  for (size_t i = 0; i < count; ++i)
  {
    if (output[i].min.x == 70)
    {
      check_area(&output[i], area(70, 0, 71, 0));
      found = true;
    }
  }
  assert(found);
  check_coverage(&collection, inputs, ARRAY_SIZE(inputs));
}

static void test14(void)
{
  /* Merge existing pair */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  static MapArea const inputs[] = {
    {.min = {0, 0}, .max = {0, 0}}, {.min = {2, 0}, .max = {2, 0}},
    {.min = {20, 0}, .max = {20, 0}}, {.min = {40, 0}, .max = {40, 0}},
    {.min = {60, 0}, .max = {60, 0}}, {.min = {80, 0}, .max = {80, 0}},
    {.min = {100, 0}, .max = {100, 0}}, {.min = {120, 0}, .max = {120, 0}},
    {.min = {0, 100}, .max = {0, 100}}
  };
  for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i)
  {
    MapAreaCol_add(&collection, &inputs[i]);
  }
  MapArea output[MapAreaColMax];
  size_t const count = collect(&collection, output);
  assert(count == MapAreaColMax);
  bool found = false;
  for (size_t i = 0; i < count; ++i)
  {
    if (output[i].min.y == 100)
    {
      check_area(&output[i], inputs[ARRAY_SIZE(inputs) - 1]);
      found = true;
    }
  }
  assert(found);
  check_coverage(&collection, inputs, ARRAY_SIZE(inputs));
}

static void test15(void)
{
  /* Repeated capacity pressure */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea inputs[40];
  for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i)
  {
    MapCoord const x = (MapCoord)(i % 8) * 10, y = (MapCoord)(i / 8) * 10;
    inputs[i] = area(x, y, x + 1, y + 1);
    MapAreaCol_add(&collection, &inputs[i]);
    check_coverage(&collection, inputs, i + 1);
  }
}

static void test16(void)
{
  /* Scaled capacity */
  MapAreaColData collection;
  MapAreaCol_init(&collection, MAP_COORDS_LIMIT_LOG2);
  MapArea inputs[MapAreaColMax + 1];
  for (size_t i = 0; i < ARRAY_SIZE(inputs); ++i)
  {
    MapCoord const x = (MapCoord)i * 10;
    inputs[i] = area(x, 0, x, 0);
    MapAreaCol_add(&collection, &inputs[i]);
  }
  check_coverage(&collection, inputs, ARRAY_SIZE(inputs));
}

static void test17(void)
{
  /* Quantised expansion */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 2);
  MapArea inputs[MapAreaColMax + 1];
  for (size_t i = 0; i < MapAreaColMax; ++i)
  {
    MapCoord const x = (MapCoord)i * 10;
    inputs[i] = area(x, 0, x + 3, 3);
    MapAreaCol_add(&collection, &inputs[i]);
  }
  inputs[MapAreaColMax] = area(4, 0, 4, 0);
  MapAreaCol_add(&collection, &inputs[MapAreaColMax]);
  check_coverage(&collection, inputs, ARRAY_SIZE(inputs));
}

static void test18(void)
{
  /* Repeated end iterator */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapAreaColIter iter;
  assert(!MapAreaColIter_get_first(&iter, &collection));
  assert(!MapAreaColIter_get_next(&iter));
  assert(!MapAreaColIter_get_next(&iter));
}

static void test19(void)
{
  /* Reinitialise */
  MapAreaColData collection;
  MapAreaCol_init(&collection, 0);
  MapArea const a = area(0, 0, 2, 2);
  MapAreaCol_add(&collection, &a);
  MapAreaCol_init(&collection, MAP_COORDS_LIMIT_LOG2);
  MapAreaColIter iter;
  assert(!MapAreaColIter_get_first(&iter, &collection));
}

void MapAreaCol_tests(void)
{
  /* MapAreaCol */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initialise", test1 },
    { "Add rectangle", test2 },
    { "Duplicate rectangle", test3 },
    { "Contained rectangle", test4 },
    { "Containing rectangle", test5 },
    { "Merge overlap", test6 },
    { "Merge touching edges", test7 },
    { "Merge touching corners", test8 },
    { "Keep disjoint rectangles", test9 },
    { "Bridge rectangles", test10 },
    { "Cascade bounding merges", test11 },
    { "Fill capacity", test12 },
    { "Extend nearest rectangle", test13 },
    { "Merge existing pair", test14 },
    { "Repeated capacity pressure", test15 },
    { "Scaled capacity", test16 },
    { "Quantised expansion", test17 },
    { "Repeated end iterator", test18 },
    { "Reinitialise", test19 },
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
