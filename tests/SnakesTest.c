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

#ifdef FORTIFY
static void test6(void)
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
#ifdef FORTIFY
    { "Snake allocation failures preserve loaded prefix", test6 },
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
