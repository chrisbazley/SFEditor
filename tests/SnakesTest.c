/* SFEditor test: snake definitions and plotting.
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
#include "../SFError.h"
#include "../Snakes.h"
#include "../SnakesData.h"
#include "../MapCoord.h"

enum {
  GridSize = 16, NameSize = 16,
  North = 1, East = 2, South = 4, West = 8, Inside = 16,
  Directions = North | East | South | West,
  PartCount = 32,
  Background = 200,
  TileCount = 64,
};

typedef struct
{
  SnakeContext ctx;
  unsigned writes;
  unsigned char cells[GridSize][GridSize];
} PlotFixture;

static FILE *open_text(char const *text)
{
  _Optional FILE *const file = tmpfile();
  assert(file);
  assert(fputs(text, &*file) >= 0);
  rewind(&*file);
  return &*file;
}

static SFError load_text(Snakes *snakes, char const *text,
  char (*error)[ErrBufferSize])
{
  FILE *const file = open_text(text);
  SFError const err = Snakes_load(file, snakes, TileCount, *error);
  assert(fclose(file) == 0);
  return err;
}

/* Each fixture tile number encodes the exits declared in the text file.
 * That lets the checks inspect connectivity without reproducing plotting. */
static void write_definition(FILE *file, char const *name, int side)
{
  assert(fprintf(file, "StartSnake '%s'\n", name) > 0);

  for (unsigned part = 0; part < PartCount; ++part)
  {
    if ((side == 0) && ((part & Inside) != 0))
    {
      continue;
    }
    if ((side == 1) && ((part & Inside) == 0))
    {
      continue;
    }

    assert(fprintf(file, "%u:%d,%d,%d,%d,%d\n", part + 1,
      (part & North) != 0, (part & East) != 0, (part & South) != 0,
      (part & West) != 0, (part & Inside) != 0) > 0);
  }

  assert(fputs("EndSnake\n", file) >= 0);
}

static void load_definition(Snakes *snakes, int side)
{
  _Optional FILE *const file = tmpfile();
  assert(file);
  write_definition(&*file, "Road", side);
  rewind(&*file);
  char error[ErrBufferSize];
  assert(!SFError_fail(Snakes_load(&*file, snakes, TileCount, error)));
  assert(error[0] == '\0');
  assert(fclose(&*file) == 0);
}

static void expect_error(char const *text, SFErrorType expected, char const *detail)
{
  Snakes snakes;
  Snakes_init(&snakes);
  char error[ErrBufferSize];

  SFError const err = load_text(&snakes, text, &error);

  assert(err.type == expected);
  assert(strcmp(error, detail) == 0);

  Snakes_free(&snakes);
}

static bool is_piece(unsigned char tile)
{
  return (tile >= 1) && (tile <= PartCount);
}

static unsigned exits(unsigned char tile)
{
  assert(is_piece(tile));
  return tile - 1;
}

static unsigned char read_cell(MapPoint pos, SnakeContext *ctx)
{
  PlotFixture const *const fixture = CONTAINER_OF(ctx, PlotFixture, ctx);
  if ((pos.x < 0) || (pos.x >= GridSize) || (pos.y < 0) || (pos.y >= GridSize))
  {
    return Background;
  }
  return fixture->cells[pos.y][pos.x];
}

static void write_cell(MapPoint pos, unsigned char tile, SnakeContext *ctx)
{
  PlotFixture *const fixture = CONTAINER_OF(ctx, PlotFixture, ctx);
  assert(pos.x >= 0);
  assert(pos.x < GridSize);
  assert(pos.y >= 0);
  assert(pos.y < GridSize);
  assert(is_piece(tile));
  fixture->cells[pos.y][pos.x] = tile;
  ++fixture->writes;
}

static void init_plot(PlotFixture *fixture)
{
  /* Snakes_begin_line initialises the callback context before using it. */
  fixture->writes = 0;
  memset(fixture->cells, Background, sizeof(fixture->cells));
}

static void check_grid(PlotFixture const *fixture, MapArea allowed, MapCoord expected)
{
  static const struct
  {
    MapPoint delta;
    unsigned exit, opposite;
  } neighbours[] = {
    {.delta = {0, 1}, .exit = North, .opposite = South},
    {.delta = {1, 0}, .exit = East, .opposite = West},
    {.delta = {0, -1}, .exit = South, .opposite = North},
    {.delta = {-1, 0}, .exit = West, .opposite = East},
  };
  MapCoord count = 0;

  for (MapCoord y = 0; y < GridSize; ++y)
  {
    for (MapCoord x = 0; x < GridSize; ++x)
    {
      unsigned char const tile = fixture->cells[y][x];
      if (!is_piece(tile))
      {
        assert(tile == Background);
        continue;
      }

      assert(x >= allowed.min.x);
      assert(x <= allowed.max.x);
      assert(y >= allowed.min.y);
      assert(y <= allowed.max.y);
      ++count;

      for (size_t i = 0; i < ARRAY_SIZE(neighbours); ++i)
      {
        if ((exits(tile) & neighbours[i].exit) == 0)
        {
          continue;
        }

        MapPoint const next = {x + neighbours[i].delta.x, y + neighbours[i].delta.y};
        assert(next.x >= 0);
        assert(next.x < GridSize);
        assert(next.y >= 0);
        assert(next.y < GridSize);
        unsigned char const other = fixture->cells[next.y][next.x];
        assert(is_piece(other));
        assert((exits(other) & neighbours[i].opposite) != 0);
      }
    }
  }

  assert(count == expected);
}

static void check_path(PlotFixture const *fixture, MapPoint start, MapPoint end)
{
  MapCoord dx = end.x - start.x, dy = end.y - start.y;
  if (dx < 0)
  {
    dx = -dx;
  }
  if (dy < 0)
  {
    dy = -dy;
  }

  MapArea const area = {
    .min = {LOWEST(start.x, end.x), LOWEST(start.y, end.y)},
    .max = {HIGHEST(start.x, end.x), HIGHEST(start.y, end.y)},
  };
  check_grid(fixture, area, dx + dy + 1);
  assert(is_piece(fixture->cells[start.y][start.x]));
  assert(is_piece(fixture->cells[end.y][end.x]));
}

static FILE *open_many(int count)
{
  _Optional FILE *const file = tmpfile();
  assert(file);

  for (int i = 0; i < count; ++i)
  {
    assert(fprintf(&*file, "StartSnake 'Snake%d'\nEndSnake\n", i) > 0);
  }

  rewind(&*file);
  return &*file;
}

static void check_names(Snakes const *snakes, int count)
{
  assert(Snakes_get_count(snakes) == count);

  for (int i = 0; i < count; ++i)
  {
    char actual[NameSize], expected[NameSize];
    assert(snprintf(expected, sizeof(expected), "Snake%d", i) > 0);
    Snakes_get_name(snakes, i, actual, NameSize);
    assert(strcmp(actual, expected) == 0);
  }
}

static void test1(void)
{
  /* Initial snake count */

  Snakes snakes;
  Snakes_init(&snakes);

  assert(Snakes_get_count(&snakes) == 0);

  Snakes_free(&snakes);
}

static void test2(void)
{
  /* Full snake definition */

  Snakes snakes;
  Snakes_init(&snakes);

  load_definition(&snakes, -1);

  assert(Snakes_get_count(&snakes) == 1);
  assert(Snakes_has_junctions(&snakes, 0));
  assert(Snakes_has_bends(&snakes, 0));
  char name[NameSize];
  Snakes_get_name(&snakes, 0, name, NameSize);
  assert(strcmp(name, "Road") == 0);

  Snakes_free(&snakes);
}

static void test3(void)
{
  /* Bounded snake name */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);

  for (int capacity = 1; capacity <= 5; ++capacity)
  {
    char name[7] = {'L', '?', '?', '?', '?', '?', 'R'};

    Snakes_get_name(&snakes, 0, name + 1, capacity);

    assert(name[0] == 'L');
    assert(name[capacity + 1] == (capacity == 5 ? 'R' : '?'));
    assert(name[capacity] == '\0');
    assert(memcmp(name + 1, "Road", capacity - 1) == 0);
  }

  Snakes_free(&snakes);
}

static void test4(void)
{
  /* Zero-capacity snake name */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  char name[] = "unchanged";

  Snakes_get_name(&snakes, 0, name, 0);

  assert(strcmp(name, "unchanged") == 0);

  Snakes_free(&snakes);
}

static void test5(void)
{
  /* Snake storage growth */

  Snakes snakes;
  Snakes_init(&snakes);
  FILE *const file = open_many(17);
  char error[ErrBufferSize];

  assert(!SFError_fail(Snakes_load(file, &snakes, TileCount, error)));

  check_names(&snakes, 17);

  assert(fclose(file) == 0);
  Snakes_free(&snakes);
}

static void test6(void)
{
  /* Reload replaces snake definitions */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  char error[ErrBufferSize];

  assert(!SFError_fail(load_text(&snakes, "# Empty definitions\n\n", &error)));

  assert(Snakes_get_count(&snakes) == 0);

  Snakes_free(&snakes);
}

static void test7(void)
{
  /* Snake definition whitespace */

  Snakes snakes;
  Snakes_init(&snakes);
  char error[ErrBufferSize];
  char const text[] = "# Comment\n\nStartSnake 'Road'  \t\n1:0,0,0,0,0\nEndSnake   \t";

  assert(!SFError_fail(load_text(&snakes, text, &error)));

  assert(Snakes_get_count(&snakes) == 1);
  assert(!Snakes_has_junctions(&snakes, 0));
  assert(!Snakes_has_bends(&snakes, 0));

  Snakes_free(&snakes);
}

static void test8(void)
{
  /* Unmatched snake terminator */

  expect_error("EndSnake\n", SFErrorType_Unexp, "1");
}

static void test9(void)
{
  /* Nested snake definition */

  expect_error("StartSnake 'Road'\nStartSnake 'Other'\n", SFErrorType_Unexp, "2");
}

static void test10(void)
{
  /* Missing snake name */

  expect_error("StartSnake\n", SFErrorType_Mistake, "1");
}

static void test11(void)
{
  /* Overlong snake name */

  expect_error("StartSnake '1234567890123456'\n", SFErrorType_StringTooLong, "1");
}

static void test12(void)
{
  /* Missing snake terminator */

  expect_error("StartSnake 'Road'\n", SFErrorType_EOF, "EndSnake\n");
}

static void test13(void)
{
  /* Piece outside snake definition */

  expect_error("1:0,0,0,0,0\n", SFErrorType_Mistake, "1");
}

static void test14(void)
{
  /* Incomplete snake piece */

  expect_error("StartSnake 'Road'\n1:0,0\n", SFErrorType_Mistake, "2");
}

static void test15(void)
{
  /* Negative snake tile */

  expect_error("StartSnake 'Road'\n-1:0,0,0,0,0\n", SFErrorType_NumRange, "2");
}

static void test16(void)
{
  /* Out-of-range snake tile */

  expect_error("StartSnake 'Road'\n64:0,0,0,0,0\n", SFErrorType_NumRange, "2");
}

static void test17(void)
{
  /* Invalid north snake flag */

  expect_error("StartSnake 'Road'\n1:2,0,0,0,0\n", SFErrorType_Mistake, "2");
}

static void test18(void)
{
  /* Invalid east snake flag */

  expect_error("StartSnake 'Road'\n1:0,2,0,0,0\n", SFErrorType_Mistake, "2");
}

static void test19(void)
{
  /* Invalid south snake flag */

  expect_error("StartSnake 'Road'\n1:0,0,2,0,0\n", SFErrorType_Mistake, "2");
}

static void test20(void)
{
  /* Invalid west snake flag */

  expect_error("StartSnake 'Road'\n1:0,0,0,2,0\n", SFErrorType_Mistake, "2");
}

static void test21(void)
{
  /* Invalid inside snake flag */

  expect_error("StartSnake 'Road'\n1:0,0,0,0,2\n", SFErrorType_Mistake, "2");
}

static void test22(void)
{
  /* Maximum snake name length */

  Snakes snakes;
  Snakes_init(&snakes);
  char error[ErrBufferSize];

  assert(!SFError_fail(load_text(&snakes, "StartSnake '123456789012345'\nEndSnake\n", &error)));

  char name[NameSize];
  Snakes_get_name(&snakes, 0, name, NameSize);
  assert(strcmp(name, "123456789012345") == 0);

  Snakes_free(&snakes);
}

static void test23(void)
{
  /* Snake line connectivity */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  MapPoint const start = {8, 8};

  for (MapCoord dy = -5; dy <= 5; ++dy)
  {
    for (MapCoord dx = -5; dx <= 5; ++dx)
    {
      static PlotFixture fixture;
      init_plot(&fixture);
      MapPoint const end = {start.x + dx, start.y + dy};
      Snakes_begin_line(&fixture.ctx, &snakes, start, 0, false, read_cell, write_cell);

      Snakes_plot_line(&fixture.ctx, end);

      check_path(&fixture, start, end);
    }
  }

  Snakes_free(&snakes);
}

static void test24(void)
{
  /* Inside snake line connectivity */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  MapPoint const start = {8, 8};

  for (MapCoord dy = -5; dy <= 5; ++dy)
  {
    for (MapCoord dx = -5; dx <= 5; ++dx)
    {
      static PlotFixture fixture;
      init_plot(&fixture);
      MapPoint const end = {start.x + dx, start.y + dy};
      Snakes_begin_line(&fixture.ctx, &snakes, start, 0, true, read_cell, write_cell);

      Snakes_plot_line(&fixture.ctx, end);

      check_path(&fixture, start, end);
    }
  }

  Snakes_free(&snakes);
}

static void test25(void)
{
  /* Inside straight edging */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  static PlotFixture fixture;
  init_plot(&fixture);
  Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){3, 8}, 0, true, read_cell, write_cell);

  Snakes_plot_line(&fixture.ctx, (MapPoint){13, 8});

  check_path(&fixture, (MapPoint){3, 8}, (MapPoint){13, 8});
  for (MapCoord x = 3; x <= 13; ++x)
  {
    assert(((exits(fixture.cells[8][x]) & Inside) != 0) == true);
  }

  Snakes_free(&snakes);
}

static void test26(void)
{
  /* Outside straight edging */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  static PlotFixture fixture;
  init_plot(&fixture);
  Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){3, 8}, 0, false, read_cell, write_cell);

  Snakes_plot_line(&fixture.ctx, (MapPoint){13, 8});

  check_path(&fixture, (MapPoint){3, 8}, (MapPoint){13, 8});
  for (MapCoord x = 3; x <= 13; ++x)
  {
    assert(((exits(fixture.cells[8][x]) & Inside) != 0) == false);
  }

  Snakes_free(&snakes);
}

static void test27(void)
{
  /* Snake line corner */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  static PlotFixture fixture;
  init_plot(&fixture);
  Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){3, 3}, 0, false, read_cell, write_cell);
  Snakes_plot_line(&fixture.ctx, (MapPoint){8, 3});

  Snakes_plot_line(&fixture.ctx, (MapPoint){8, 8});

  check_path(&fixture, (MapPoint){3, 3}, (MapPoint){8, 8});
  assert((exits(fixture.cells[3][8]) & Directions) == (North | West));

  Snakes_free(&snakes);
}

static void test28(void)
{
  /* Crossing snake lines */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  static PlotFixture fixture;
  init_plot(&fixture);
  Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){3, 8}, 0, false, read_cell, write_cell);
  Snakes_plot_line(&fixture.ctx, (MapPoint){13, 8});

  Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){8, 3}, 0, false, read_cell, write_cell);
  Snakes_plot_line(&fixture.ctx, (MapPoint){8, 13});

  check_grid(&fixture, (MapArea){.min = {3, 3}, .max = {13, 13}}, 21);
  assert((exits(fixture.cells[8][8]) & Directions) == Directions);

  Snakes_free(&snakes);
}

static void test29(void)
{
  /* Inside uses available outside pieces */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, 0);
  static PlotFixture fixture;
  init_plot(&fixture);

  int const tile = Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){8, 8}, 0, true, read_cell, write_cell);

  assert(tile == 1);
  assert(fixture.cells[8][8] == 1);

  Snakes_free(&snakes);
}

static void test30(void)
{
  /* Outside uses available inside pieces */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, 1);
  static PlotFixture fixture;
  init_plot(&fixture);

  int const tile = Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){8, 8}, 0, false, read_cell, write_cell);

  assert(tile == 17);
  assert(fixture.cells[8][8] == 17);

  Snakes_free(&snakes);
}

static void test31(void)
{
  /* Missing snake piece avoids writing */

  Snakes snakes;
  Snakes_init(&snakes);
  char error[ErrBufferSize];
  assert(!SFError_fail(load_text(&snakes, "StartSnake 'Road'\nEndSnake\n", &error)));
  static PlotFixture fixture;
  init_plot(&fixture);

  int const tile = Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){8, 8}, 0, false, read_cell, write_cell);

  assert(tile == -1);
  assert(fixture.writes == 0);
  assert(fixture.cells[8][8] == Background);

  Snakes_free(&snakes);
}

static void test32(void)
{
  /* Snake plotting without write callback */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  static PlotFixture fixture;
  init_plot(&fixture);

  assert(Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){3, 8}, 0, false, read_cell, NULL) == 1);
  Snakes_plot_line(&fixture.ctx, (MapPoint){13, 8});

  assert(fixture.writes == 0);
  assert(fixture.ctx.map_pos.x == 13);
  assert(fixture.ctx.map_pos.y == 8);
  for (size_t y = 0; y < GridSize; ++y)
  {
    for (size_t x = 0; x < GridSize; ++x)
    {
      assert(fixture.cells[y][x] == Background);
    }
  }

  Snakes_free(&snakes);
}

static void test33(void)
{
  /* Continued snake line */

  Snakes snakes;
  Snakes_init(&snakes);
  load_definition(&snakes, -1);
  static PlotFixture fixture;
  init_plot(&fixture);
  Snakes_begin_line(&fixture.ctx, &snakes, (MapPoint){3, 8}, 0, false, read_cell, write_cell);
  Snakes_plot_line(&fixture.ctx, (MapPoint){8, 8});

  Snakes_plot_line(&fixture.ctx, (MapPoint){13, 8});

  check_path(&fixture, (MapPoint){3, 8}, (MapPoint){13, 8});

  Snakes_free(&snakes);
}

#ifdef FORTIFY
static void test34(void)
{
  /* Snake allocation failures preserve loaded prefix */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    FILE *const file = open_many(9);
    Snakes snakes;
    Snakes_init(&snakes);
    char error[ErrBufferSize];

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = Snakes_load(file, &snakes, TileCount, error);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
      assert(Snakes_get_count(&snakes) < 9);
      check_names(&snakes, Snakes_get_count(&snakes));
      assert(error[0] != '\0');
    }
    else
    {
      check_names(&snakes, 9);
      succeeded = true;
    }

    Snakes_free(&snakes);
    assert(fclose(file) == 0);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);
}

#endif

void Snakes_tests(void)
{
  /* Snakes */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial snake count", test1 },
    { "Full snake definition", test2 },
    { "Bounded snake name", test3 },
    { "Zero-capacity snake name", test4 },
    { "Snake storage growth", test5 },
    { "Reload replaces snake definitions", test6 },
    { "Snake definition whitespace", test7 },
    { "Unmatched snake terminator", test8 },
    { "Nested snake definition", test9 },
    { "Missing snake name", test10 },
    { "Overlong snake name", test11 },
    { "Missing snake terminator", test12 },
    { "Piece outside snake definition", test13 },
    { "Incomplete snake piece", test14 },
    { "Negative snake tile", test15 },
    { "Out-of-range snake tile", test16 },
    { "Invalid north snake flag", test17 },
    { "Invalid east snake flag", test18 },
    { "Invalid south snake flag", test19 },
    { "Invalid west snake flag", test20 },
    { "Invalid inside snake flag", test21 },
    { "Maximum snake name length", test22 },
    { "Snake line connectivity", test23 },
    { "Inside snake line connectivity", test24 },
    { "Inside straight edging", test25 },
    { "Outside straight edging", test26 },
    { "Snake line corner", test27 },
    { "Crossing snake lines", test28 },
    { "Inside uses available outside pieces", test29 },
    { "Outside uses available inside pieces", test30 },
    { "Missing snake piece avoids writing", test31 },
    { "Snake plotting without write callback", test32 },
    { "Continued snake line", test33 },
#ifdef FORTIFY
    { "Snake allocation failures preserve loaded prefix", test34 },
#endif
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
