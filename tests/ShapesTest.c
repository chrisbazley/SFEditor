/* SFEditor test: shape drawing.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../Shapes.h"

enum { Side = 81, Origin = Side / 2, CellCount = Side * Side,
       Target = 1, Filled = 2, Barrier = 255 };

typedef struct
{
  unsigned char pixels[Side][Side];
  bool painted[Side][Side];
  size_t writes;
  bool wrap;
} Grid;

static MapCoord wrap_index(MapCoord value)
{
  return (value % Side + Side) % Side;
}

static unsigned char read_pixel(MapPoint p, void *arg)
{
  Grid const *const grid = arg;
  MapCoord x = p.x + Origin, y = p.y + Origin;
  if (grid->wrap)
  {
    x = wrap_index(x);
    y = wrap_index(y);
  }
  else if (x < 0 || x >= Side || y < 0 || y >= Side)
  {
    return Barrier;
  }
  return grid->pixels[y][x];
}

static void paint(MapArea const *area, void *arg)
{
  Grid *const grid = arg;
  assert(MapArea_is_valid(area));
  if (!grid->wrap)
  {
    assert(area->min.x >= -Origin);
    assert(area->min.y >= -Origin);
    assert(area->max.x <= Origin);
    assert(area->max.y <= Origin);
  }
  for (MapCoord y = area->min.y; y <= area->max.y; ++y)
  {
    for (MapCoord x = area->min.x; x <= area->max.x; ++x)
    {
      MapCoord const ix = wrap_index(x + Origin), iy = wrap_index(y + Origin);
      grid->pixels[iy][ix] = Filled;
      grid->painted[iy][ix] = true;
    }
  }
  ++grid->writes;
}

static void put_rectangle(Grid *grid, MapArea area, unsigned char value)
{
  for (MapCoord y = area.min.y; y <= area.max.y; ++y)
  {
    for (MapCoord x = area.min.x; x <= area.max.x; ++x)
    {
      assert(x >= -Origin);
      assert(x <= Origin);
      assert(y >= -Origin);
      assert(y <= Origin);
      grid->pixels[y + Origin][x + Origin] = value;
    }
  }
}

static void expect_pixel(Grid *grid, MapPoint p)
{
  assert(p.x >= -Origin);
  assert(p.x <= Origin);
  assert(p.y >= -Origin);
  assert(p.y <= Origin);
  grid->painted[p.y + Origin][p.x + Origin] = true;
}

static void compare_paint(Grid const *actual, Grid const *expected)
{
  for (size_t y = 0; y < Side; ++y)
  {
    for (size_t x = 0; x < Side; ++x)
    {
      assert(actual->painted[y][x] == expected->painted[y][x]);
    }
  }
}

/* Independent four-neighbour queue search, rather than a scanline fill. */
static void flood_reference(Grid const *source, MapPoint seed, Grid *expected)
{
  static MapPoint queue[CellCount];
  static MapPoint const neighbours[] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  size_t head = 0, tail = 0;
  MapPoint const first = {seed.x + Origin, seed.y + Origin};
  assert(source->pixels[first.y][first.x] == Target);
  queue[tail++] = first;
  expected->painted[first.y][first.x] = true;
  while (head < tail)
  {
    MapPoint const p = queue[head++];
    for (size_t i = 0; i < ARRAY_SIZE(neighbours); ++i)
    {
      MapCoord x = p.x + neighbours[i].x, y = p.y + neighbours[i].y;
      if (source->wrap)
      {
        x = wrap_index(x);
        y = wrap_index(y);
      }
      else if (x < 0 || x >= Side || y < 0 || y >= Side)
      {
        continue;
      }
      if (expected->painted[y][x] || source->pixels[y][x] != Target)
      {
        continue;
      }
      assert(tail < ARRAY_SIZE(queue));
      queue[tail++] = (MapPoint){x, y};
      expected->painted[y][x] = true;
    }
  }
}

static void check_flood(Grid const *source, MapPoint seed)
{
  Grid actual = *source, expected = {0};
  flood_reference(source, seed, &expected);
  assert(Shapes_flood(read_pixel, paint, &actual, Target, seed, Side));
  compare_paint(&actual, &expected);
  for (size_t y = 0; y < Side; ++y)
  {
    for (size_t x = 0; x < Side; ++x)
    {
      unsigned char const value = expected.painted[y][x] ? Filled : source->pixels[y][x];
      assert(actual.pixels[y][x] == value);
    }
  }
}

static void make_comb(Grid *grid)
{
  put_rectangle(grid, (MapArea){.min = {-39, 0}, .max = {39, 0}}, Target);
  put_rectangle(grid, (MapArea){.min = {-39, 2}, .max = {39, 2}}, Target);
  for (MapCoord x = -39; x <= 39; x += 2)
  {
    put_rectangle(grid, (MapArea){.min = {x, 1}, .max = {x, 1}}, Target);
  }
}

/* Raster boundaries have no single ideal pixel choice. Require all pixels
 * well inside the stroke and reject pixels beyond a 1.25-pixel boundary band. */
static void check_stroke(MapPoint a, MapPoint b, MapCoord radius)
{
  Grid actual = {0};
  Shapes_line(paint, &actual, a, b, radius);
  assert(actual.painted[a.y + Origin][a.x + Origin]);
  assert(actual.painted[b.y + Origin][b.x + Origin]);
  double const dx = (double)(b.x - a.x), dy = (double)(b.y - a.y);
  double const length_squared = dx * dx + dy * dy;
  for (MapCoord y = -Origin; y <= Origin; ++y)
  {
    for (MapCoord x = -Origin; x <= Origin; ++x)
    {
      double t = length_squared ? ((x - a.x) * dx + (y - a.y) * dy) / length_squared : 0;
      if (t < 0)
      {
        t = 0;
      }
      if (t > 1)
      {
        t = 1;
      }
      double const px = x - (a.x + t * dx), py = y - (a.y + t * dy);
      double const distance = sqrt(px * px + py * py);
      bool const painted = actual.painted[y + Origin][x + Origin];
      if (painted)
      {
        assert(distance <= radius + 1.25);
      }
      if (distance < radius - 1.25)
      {
        assert(painted);
      }
    }
  }
}

/* For awkward triangles, allow the same boundary band on each edge.
 * Exact expected masks are reserved for the simple triangles below. */
static void check_triangle(MapPoint a, MapPoint b, MapPoint c)
{
  Grid actual = {0};
  Shapes_tri(paint, &actual, a, b, c);
  assert(actual.writes > 0);
  MapCoord const xmin = LOWEST(a.x, LOWEST(b.x, c.x));
  MapCoord const xmax = HIGHEST(a.x, HIGHEST(b.x, c.x));
  MapCoord const ymin = LOWEST(a.y, LOWEST(b.y, c.y));
  MapCoord const ymax = HIGHEST(a.y, HIGHEST(b.y, c.y));
  double const cross = (double)(b.x - a.x) * (c.y - a.y) - (double)(b.y - a.y) * (c.x - a.x);
  MapPoint const vertices[] = {a, b, c};
  for (MapCoord y = -Origin; y <= Origin; ++y)
  {
    for (MapCoord x = -Origin; x <= Origin; ++x)
    {
      bool const painted = actual.painted[y + Origin][x + Origin];
      if (painted)
      {
        assert(x >= xmin);
        assert(x <= xmax);
        assert(y >= ymin);
        assert(y <= ymax);
      }
      bool interior = cross != 0;
      for (size_t i = 0; i < ARRAY_SIZE(vertices); ++i)
      {
        MapPoint const p = vertices[i], q = vertices[(i + 1) % ARRAY_SIZE(vertices)];
        double const dx = (double)(q.x - p.x), dy = (double)(q.y - p.y);
        double const length = sqrt(dx * dx + dy * dy);
        if (cross == 0 || length == 0)
        {
          continue;
        }
        double const distance = ((x - p.x) * dy - (y - p.y) * dx) * (cross < 0 ? 1 : -1) / length;
        if (distance < 1.25)
        {
          interior = false;
        }
        if (painted)
        {
          assert(distance >= -1.25);
        }
      }
      if (interior)
      {
        assert(painted);
      }
    }
  }
}

static void test1(void)
{
  /* Rectangle */
  Grid actual = {0}, expected = {0};
  Shapes_rect(paint, &actual, (MapPoint){-2, -1}, (MapPoint){2, 1});
  for (MapCoord y = -1; y <= 1; ++y)
  {
    for (MapCoord x = -2; x <= 2; ++x)
    {
      expect_pixel(&expected, (MapPoint){x, y});
    }
  }
  compare_paint(&actual, &expected);
}

static void test2(void)
{
  /* Reversed rectangle */
  Grid actual = {0}, expected = {0};
  Shapes_rect(paint, &actual, (MapPoint){2, 1}, (MapPoint){-2, -1});
  for (MapCoord y = -1; y <= 1; ++y)
  {
    for (MapCoord x = -2; x <= 2; ++x)
    {
      expect_pixel(&expected, (MapPoint){x, y});
    }
  }
  compare_paint(&actual, &expected);
}

static void test3(void)
{
  /* Point rectangle */
  Grid actual = {0}, expected = {0};
  Shapes_rect(paint, &actual, (MapPoint){-3, 2}, (MapPoint){-3, 2});
  expect_pixel(&expected, (MapPoint){-3, 2});
  compare_paint(&actual, &expected);
}

static void test4(void)
{
  /* Horizontal line */
  Grid actual = {0}, expected = {0};
  Shapes_line(paint, &actual, (MapPoint){-3, 2}, (MapPoint){3, 2}, 0);
  for (MapCoord i = -3; i <= 3; ++i)
  {
    expect_pixel(&expected, (MapPoint){i, 2});
  }
  compare_paint(&actual, &expected);
}

static void test5(void)
{
  /* Vertical line */
  Grid actual = {0}, expected = {0};
  Shapes_line(paint, &actual, (MapPoint){2, -3}, (MapPoint){2, 3}, 0);
  for (MapCoord i = -3; i <= 3; ++i)
  {
    expect_pixel(&expected, (MapPoint){2, i});
  }
  compare_paint(&actual, &expected);
}

static void test6(void)
{
  /* Point line */
  Grid actual = {0}, expected = {0};
  Shapes_line(paint, &actual, (MapPoint){-3, 2}, (MapPoint){-3, 2}, 0);
  expect_pixel(&expected, (MapPoint){-3, 2});
  compare_paint(&actual, &expected);
}

static void test7(void)
{
  /* Line octants */
  static MapPoint const pixels[] = {{0, 0}, {1, 0}, {2, 1}, {3, 1}, {4, 2}, {5, 2}};
  for (int swap = 0; swap <= 1; ++swap)
  {
    for (int sx = -1; sx <= 1; sx += 2)
    {
      for (int sy = -1; sy <= 1; sy += 2)
      {
        Grid actual = {0}, expected = {0};
        MapPoint const end = {sx * (swap ? 2 : 5), sy * (swap ? 5 : 2)};
        Shapes_line(paint, &actual, (MapPoint){0, 0}, end, 0);
        for (size_t i = 0; i < ARRAY_SIZE(pixels); ++i)
        {
          MapPoint const p = {sx * (swap ? pixels[i].y : pixels[i].x),
                              sy * (swap ? pixels[i].x : pixels[i].y)};
          expect_pixel(&expected, p);
        }
        compare_paint(&actual, &expected);
      }
    }
  }
}

static void test8(void)
{
  /* Line reversal */
  static MapPoint const ends[] = {{7, 3}, {-7, 3}, {3, 7}, {-3, 7}, {7, -3}, {-7, -3}, {3, -7}, {-3, -7}};
  for (MapCoord thickness = 0; thickness <= 3; ++thickness)
  {
    for (size_t i = 0; i < ARRAY_SIZE(ends); ++i)
    {
      Grid forward = {0}, reverse = {0};
      Shapes_line(paint, &forward, (MapPoint){0, 0}, ends[i], thickness);
      Shapes_line(paint, &reverse, ends[i], (MapPoint){0, 0}, thickness);
      compare_paint(&forward, &reverse);
    }
  }
}

static void test9(void)
{
  /* Thick horizontal line */
  Grid actual = {0}, expected = {0};
  Shapes_line(paint, &actual, (MapPoint){-2, 0}, (MapPoint){2, 0}, 1);
  for (MapCoord along = -2; along <= 2; ++along)
  {
    for (MapCoord across = -1; across <= 1; ++across)
    {
      expect_pixel(&expected, (MapPoint){along, across});
    }
  }
  expect_pixel(&expected, (MapPoint){-3, 0});
  expect_pixel(&expected, (MapPoint){3, 0});
  compare_paint(&actual, &expected);
}

static void test10(void)
{
  /* Thick vertical line */
  Grid actual = {0}, expected = {0};
  Shapes_line(paint, &actual, (MapPoint){0, -2}, (MapPoint){0, 2}, 1);
  for (MapCoord along = -2; along <= 2; ++along)
  {
    for (MapCoord across = -1; across <= 1; ++across)
    {
      expect_pixel(&expected, (MapPoint){across, along});
    }
  }
  expect_pixel(&expected, (MapPoint){0, -3});
  expect_pixel(&expected, (MapPoint){0, 3});
  compare_paint(&actual, &expected);
}

static void test11(void)
{
  /* Awkward thick strokes */
  static MapPoint const ends[] = {{7, 3}, {-7, 3}, {3, 7}, {-3, 7}, {7, -3}, {-7, -3}, {3, -7}, {-3, -7}};
  for (MapCoord radius = 1; radius <= 3; ++radius)
  {
    for (size_t i = 0; i < ARRAY_SIZE(ends); ++i)
    {
      check_stroke((MapPoint){0, 0}, ends[i], radius);
    }
  }
}

static void test12(void)
{
  /* Degenerate thick stroke */
  for (MapCoord radius = 1; radius <= 3; ++radius)
  {
    check_stroke((MapPoint){-2, 3}, (MapPoint){-2, 3}, radius);
  }
}

static void test13(void)
{
  /* Point circle */
  Grid actual = {0}, expected = {0};
  Shapes_circ(paint, &actual, (MapPoint){-2, 3}, 0);
  expect_pixel(&expected, (MapPoint){-2, 3});
  compare_paint(&actual, &expected);
}

static void test14(void)
{
  /* Unit circle */
  Grid actual = {0}, expected = {0};
  Shapes_circ(paint, &actual, (MapPoint){0, 0}, 1);
  static MapPoint const pixels[] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  for (size_t i = 0; i < ARRAY_SIZE(pixels); ++i)
  {
    expect_pixel(&expected, pixels[i]);
  }
  compare_paint(&actual, &expected);
}

static void test15(void)
{
  /* Circle boundary band */
  for (MapCoord radius = 2; radius <= 12; ++radius)
  {
    Grid actual = {0};
    Shapes_circ(paint, &actual, (MapPoint){-2, 3}, radius);
    for (MapCoord y = -Origin; y <= Origin; ++y)
    {
      for (MapCoord x = -Origin; x <= Origin; ++x)
      {
        double const dx = (double)x + 2, dy = (double)y - 3;
        double const distance = sqrt(dx * dx + dy * dy);
        bool const painted = actual.painted[y + Origin][x + Origin];
        if (distance < radius - 0.75)
        {
          assert(painted);
        }
        if (painted)
        {
          assert(distance <= radius + 0.75);
        }
      }
    }
  }
}

static void test16(void)
{
  /* Flat-bottom triangle */
  Grid actual = {0}, expected = {0};
  Shapes_tri(paint, &actual, (MapPoint){0, 0}, (MapPoint){0, 4}, (MapPoint){4, 0});
  for (MapCoord y = 0; y <= 4; ++y)
  {
    for (MapCoord x = 0; x <= 4 - y; ++x)
    {
      expect_pixel(&expected, (MapPoint){x, y});
    }
  }
  compare_paint(&actual, &expected);
}

static void test17(void)
{
  /* Flat-top triangle */
  Grid actual = {0}, expected = {0};
  Shapes_tri(paint, &actual, (MapPoint){0, 0}, (MapPoint){0, 4}, (MapPoint){4, 4});
  for (MapCoord y = 0; y <= 4; ++y)
  {
    for (MapCoord x = 0; x <= y; ++x)
    {
      expect_pixel(&expected, (MapPoint){x, y});
    }
  }
  compare_paint(&actual, &expected);
}

static void test18(void)
{
  /* Point triangle */
  Grid actual = {0}, expected = {0};
  Shapes_tri(paint, &actual, (MapPoint){2, -3}, (MapPoint){2, -3}, (MapPoint){2, -3});
  expect_pixel(&expected, (MapPoint){2, -3});
  compare_paint(&actual, &expected);
}

static void test19(void)
{
  /* Triangle boundary band */
  static MapPoint const cases[][3] = {
    {{-6, -5}, {7, -2}, {-1, 8}}, {{-7, 0}, {7, 1}, {0, 2}},
    {{-4, 7}, {0, -6}, {2, 5}}, {{-3, -3}, {3, 0}, {0, 3}}
  };
  static size_t const permutations[][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
  {
    for (size_t p = 0; p < ARRAY_SIZE(permutations); ++p)
    {
      check_triangle(cases[i][permutations[p][0]], cases[i][permutations[p][1]], cases[i][permutations[p][2]]);
    }
  }
}

static void test20(void)
{
  /* Collinear triangle */
  static MapPoint const cases[][3] = {
    {{-3, 2}, {0, 2}, {3, 2}}, {{2, -3}, {2, 0}, {2, 3}},
    {{-3, -3}, {0, 0}, {3, 3}}
  };
  for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
  {
    check_triangle(cases[i][0], cases[i][1], cases[i][2]);
  }
}

static void test21(void)
{
  /* Flood isolated point */
  Grid source = {0};
  put_rectangle(&source, (MapArea){.min = {0, 0}, .max = {0, 0}}, Target);
  check_flood(&source, (MapPoint){0, 0});
}

static void test22(void)
{
  /* Flood rectangle */
  Grid source = {0};
  put_rectangle(&source, (MapArea){.min = {-5, -3}, .max = {7, 4}}, Target);
  check_flood(&source, (MapPoint){2, 1});
}

static void test23(void)
{
  /* Flood hole */
  Grid source = {0};
  put_rectangle(&source, (MapArea){.min = {-7, -6}, .max = {7, 6}}, Target);
  put_rectangle(&source, (MapArea){.min = {-2, -2}, .max = {2, 2}}, 0);
  check_flood(&source, (MapPoint){-5, 0});
}

static void test24(void)
{
  /* Flood disconnected islands */
  Grid source = {0};
  put_rectangle(&source, (MapArea){.min = {-7, -3}, .max = {-2, 4}}, Target);
  put_rectangle(&source, (MapArea){.min = {2, -3}, .max = {7, 4}}, Target);
  check_flood(&source, (MapPoint){-5, 0});
}

static void test25(void)
{
  /* Flood diagonal contact */
  Grid source = {0};
  put_rectangle(&source, (MapArea){.min = {0, 0}, .max = {0, 0}}, Target);
  put_rectangle(&source, (MapArea){.min = {1, 1}, .max = {1, 1}}, Target);
  check_flood(&source, (MapPoint){0, 0});
}

static void test26(void)
{
  /* Flood narrow passage */
  Grid source = {0};
  put_rectangle(&source, (MapArea){.min = {-8, -5}, .max = {-3, 5}}, Target);
  put_rectangle(&source, (MapArea){.min = {3, -5}, .max = {8, 5}}, Target);
  put_rectangle(&source, (MapArea){.min = {-3, 0}, .max = {3, 0}}, Target);
  check_flood(&source, (MapPoint){-6, -2});
}

static void test27(void)
{
  /* Flood boundary */
  Grid source = {0};
  put_rectangle(&source, (MapArea){.min = {-Origin, -Origin}, .max = {Origin, Origin}}, Target);
  check_flood(&source, (MapPoint){-Origin, -Origin});
}

static void test28(void)
{
  /* Flood branching region */
  Grid source = {0};
  make_comb(&source);
  check_flood(&source, (MapPoint){0, 0});
}

static void test29(void)
{
  /* Flood wrapped seams */
  Grid source = {.wrap = true};
  put_rectangle(&source, (MapArea){.min = {-40, -40}, .max = {-39, -39}}, Target);
  put_rectangle(&source, (MapArea){.min = {39, -40}, .max = {40, -39}}, Target);
  put_rectangle(&source, (MapArea){.min = {-40, 39}, .max = {-39, 40}}, Target);
  put_rectangle(&source, (MapArea){.min = {39, 39}, .max = {40, 40}}, Target);
  put_rectangle(&source, (MapArea){.min = {0, 0}, .max = {0, 0}}, Target);
  check_flood(&source, (MapPoint){-40, -40});
}

static void test30(void)
{
  /* Flood wrapped field */
  Grid source = {.wrap = true};
  put_rectangle(&source, (MapArea){.min = {-Origin, -Origin}, .max = {Origin, Origin}}, Target);
  check_flood(&source, (MapPoint){0, 0});
}

#ifdef FORTIFY
static void test31(void)
{
  /* Flood allocation failure */
  Grid actual = {0};
  put_rectangle(&actual, (MapArea){.min = {0, 0}, .max = {0, 0}}, Target);
  Fortify_SetNumAllocationsLimit(0);
  bool const success = Shapes_flood(read_pixel, paint, &actual, Target, (MapPoint){0, 0}, Side);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(!success);
  assert(actual.writes == 0);
}

static void test32(void)
{
  /* Flood stack growth failure */
  Grid actual = {0};
  make_comb(&actual);
  Fortify_SetNumAllocationsLimit(1);
  bool const success = Shapes_flood(read_pixel, paint, &actual, Target, (MapPoint){0, 0}, Side);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(!success);
}

static void test33(void)
{
  /* Flood retry after failure */
  Grid actual = {0};
  make_comb(&actual);
  Fortify_SetNumAllocationsLimit(1);
  bool const success = Shapes_flood(read_pixel, paint, &actual, Target, (MapPoint){0, 0}, Side);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(!success);
  Grid source = {0};
  make_comb(&source);
  check_flood(&source, (MapPoint){0, 0});
}
#endif

void Shapes_tests(void)
{
  /* Shapes */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Rectangle", test1 },
    { "Reversed rectangle", test2 },
    { "Point rectangle", test3 },
    { "Horizontal line", test4 },
    { "Vertical line", test5 },
    { "Point line", test6 },
    { "Line octants", test7 },
    { "Line reversal", test8 },
    { "Thick horizontal line", test9 },
    { "Thick vertical line", test10 },
    { "Awkward thick strokes", test11 },
    { "Degenerate thick stroke", test12 },
    { "Point circle", test13 },
    { "Unit circle", test14 },
    { "Circle boundary band", test15 },
    { "Flat-bottom triangle", test16 },
    { "Flat-top triangle", test17 },
    { "Point triangle", test18 },
    { "Triangle boundary band", test19 },
    { "Collinear triangle", test20 },
    { "Flood isolated point", test21 },
    { "Flood rectangle", test22 },
    { "Flood hole", test23 },
    { "Flood disconnected islands", test24 },
    { "Flood diagonal contact", test25 },
    { "Flood narrow passage", test26 },
    { "Flood boundary", test27 },
    { "Flood branching region", test28 },
    { "Flood wrapped seams", test29 },
    { "Flood wrapped field", test30 },
#ifdef FORTIFY
    { "Flood allocation failure", test31 },
    { "Flood stack growth failure", test32 },
    { "Flood retry after failure", test33 },
#endif
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
