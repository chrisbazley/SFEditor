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

enum { Capacity = 64 };

static size_t write_data(PyramidData const *value, unsigned char (*buffer)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  pyramid_write(value, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise any space skipped at the end, without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(PyramidData *value,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = pyramid_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static SFErrorType const expected_errors[] = {
  SFErrorType_BadLevelNumber,
  SFErrorType_BadPyramid,
};

enum { ErrorCount = ARRAY_SIZE(expected_errors) };

static void note_error(SFErrorType error, bool (*seen)[ErrorCount])
{
  if (error == SFErrorType_OK)
  {
    return;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    if (error == expected_errors[i])
    {
      (*seen)[i] = true;
      return;
    }
  }
  fprintf(stderr, "Unexpected read error %d\n", (int)error);
  assert(false);
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
      unsigned char buffer[Capacity];
      size_t const size = write_data(&p, &buffer);
      assert(!SFError_fail(read_data(&loaded, &buffer, size)));
      assert(pyramid_get_difficulty(&loaded) == difficulty);
      assert(pyramid_get_level_number(&loaded) == level);
    }
  }
}

static void test3(void)
{
  /* Corrupt pyramid data */

  PyramidData p;
  pyramid_set_position(&p, Pyramid_User, 1);
  unsigned char buffer[Capacity];
  size_t const size = write_data(&p, &buffer);
  bool seen[ErrorCount] = {false};
  for (size_t offset = 0; offset < size; ++offset)
  {
    unsigned char const saved = buffer[offset];
    for (unsigned byte = 0; byte < 256; ++byte)
    {
      buffer[offset] = (unsigned char)byte;
      PyramidData loaded;
      note_error(read_data(&loaded, &buffer, size).type, &seen);
    }
    buffer[offset] = saved;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    assert(seen[i]);
  }
}

static void test4(void)
{
  /* Pyramid truncation */

  PyramidData p;
  pyramid_set_position(&p, Pyramid_User, 1);
  unsigned char buffer[Capacity];
  size_t const size = write_data(&p, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    PyramidData loaded;
    SFError const err = read_data(&loaded, &buffer, length);
    if (length == 0)
    {
      assert(SFError_fail(err));
    }
    if (!SFError_fail(err))
    {
      assert(pyramid_get_difficulty(&loaded) == Pyramid_User);
      assert(pyramid_get_level_number(&loaded) == 1);
    }
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
    { "Corrupt pyramid data", test3 },
    { "Pyramid truncation", test4 },
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
