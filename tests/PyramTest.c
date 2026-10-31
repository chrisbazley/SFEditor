/* SFEditor test: mission pyramid position.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Writer.h"
#include "WriterMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "../Pyram.h"
#include "../PyramData.h"

static SFError read_data(PyramidData *value, unsigned char const *data, size_t size)
{
  Reader reader;
  assert(reader_mem_init(&reader, data, size));
  SFError const err = pyramid_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static void make_record(unsigned char (*data)[8], int32_t level, int32_t difficulty)
{
  Writer writer;
  assert(writer_mem_init(&writer, *data, sizeof(*data)));
  writer_fwrite_int32(level, &writer);
  writer_fwrite_int32(difficulty, &writer);
  assert(writer_destroy(&writer) == 8);
}

static void test1(void)
{
  /* Pyramid position */

  for (Pyramid difficulty = Pyramid_Easy; difficulty <= Pyramid_User; ++difficulty)
  {
    for (int level = 1; level <= (difficulty == Pyramid_User ? 1 : 36); ++level)
    {
      PyramidData p;
      pyramid_set_position(&p, difficulty, level);
      assert(pyramid_get_difficulty(&p) == difficulty);
      assert(pyramid_get_level_number(&p) == level);
    }
  }
}

static void test2(void)
{
  /* Pyramid round trip */

  for (Pyramid difficulty = Pyramid_Easy; difficulty <= Pyramid_User; ++difficulty)
  {
    for (int level = 1; level <= (difficulty == Pyramid_User ? 1 : 36); ++level)
    {
      PyramidData p, loaded;
      pyramid_set_position(&p, difficulty, level);
      unsigned char data[8];
      Writer writer;
      assert(writer_mem_init(&writer, data, sizeof(data)));
      pyramid_write(&p, &writer);
      assert(writer_destroy(&writer) == sizeof(data));
      assert(data[0] == level);
      assert(data[4] == difficulty);
      for (size_t i = 0; i < 3; ++i)
      {
        assert(data[1 + i] == 0);
        assert(data[5 + i] == 0);
      }
      assert(!SFError_fail(read_data(&loaded, data, sizeof(data))));
      assert(pyramid_get_difficulty(&loaded) == difficulty);
      assert(pyramid_get_level_number(&loaded) == level);
    }
  }
}

static void test3(void)
{
  /* Invalid level number */

  static int32_t const levels[] = {-1, 0, 37, INT32_MAX};
  for (size_t i = 0; i < ARRAY_SIZE(levels); ++i)
  {
    unsigned char data[8];
    make_record(&data, levels[i], Pyramid_Easy);
    PyramidData p;
    assert(read_data(&p, data, sizeof(data)).type == SFErrorType_BadLevelNumber);
  }
  unsigned char data[8];
  make_record(&data, 2, Pyramid_User);
  PyramidData p;
  assert(read_data(&p, data, sizeof(data)).type == SFErrorType_BadLevelNumber);
}

static void test4(void)
{
  /* Invalid difficulty */

  static int32_t const difficulties[] = {-1, Pyramid_User + 1, INT32_MAX};
  for (size_t i = 0; i < ARRAY_SIZE(difficulties); ++i)
  {
    unsigned char data[8];
    make_record(&data, 1, difficulties[i]);
    PyramidData p;
    assert(read_data(&p, data, sizeof(data)).type == SFErrorType_BadPyramid);
  }
}

static void test5(void)
{
  /* Pyramid truncation */

  unsigned char data[8];
  make_record(&data, 1, Pyramid_Easy);
  for (size_t length = 0; length < sizeof(data); ++length)
  {
    PyramidData p;
    assert(read_data(&p, data, length).type == SFErrorType_ReadFail);
  }
}

void Pyram_tests(void)
{
  /* Pyram */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Pyramid position", test1 },
    { "Pyramid round trip", test2 },
    { "Invalid level number", test3 },
    { "Invalid difficulty", test4 },
    { "Pyramid truncation", test5 },
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
