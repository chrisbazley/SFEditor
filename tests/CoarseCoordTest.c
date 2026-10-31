/* SFEditor test: coarse and fine coordinates.
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
#include "../CoarseCoord.h"



static void test1(void)
{
  /* Scalar expansion */

  for (unsigned v = 0; v < 256; ++v)
  {
    assert(FineCoord_from_coarse((CoarseCoord)v) == (uint32_t)v * UINT32_C(16777216));
  }
}

static void test2(void)
{
  /* Scalar quantisation */

  for (unsigned v = 0; v < 256; ++v)
  {
    uint32_t const base = (uint32_t)v * UINT32_C(16777216);
    assert(CoarseCoord_from_fine(base) == v);
    assert(CoarseCoord_from_fine(base + 1) == v);
    assert(CoarseCoord_from_fine(base + UINT32_C(16777215)) == v);
  }
}

static void test3(void)
{
  /* Vector expansion */

  for (unsigned v = 0; v < 256; ++v)
  {
    CoarsePoint3d const input = {.x = (CoarseCoord)v, .y = (CoarseCoord)(255 - v),
                               .z = (CoarseCoord)(v ^ 85)};
    FinePoint3d const p = FinePoint3d_from_coarse(input);
    assert(p.x == (uint32_t)v * UINT32_C(16777216));
    assert(p.y == (uint32_t)(255 - v) * UINT32_C(16777216));
    assert(p.z == (uint32_t)(v ^ 85) * UINT32_C(16777216));
  }
}

static void test4(void)
{
  /* Vector quantisation */

  for (unsigned v = 0; v < 256; ++v)
  {
    FinePoint3d const input = {
      .x = (uint32_t)v * UINT32_C(16777216) + 1,
      .y = (uint32_t)(255 - v) * UINT32_C(16777216) + UINT32_C(16777215),
      .z = (uint32_t)(v ^ 85) * UINT32_C(16777216),
    };
    CoarsePoint3d const p = CoarsePoint3d_from_fine(input);
    assert(p.x == v);
    assert(p.y == 255 - v);
    assert(p.z == (v ^ 85));
  }
}

static void test5(void)
{
  /* Equal points */

  for (unsigned v = 0; v < 256; ++v)
  {
    CoarsePoint2d const p = {(CoarseCoord)v, (CoarseCoord)(255 - v)};
    assert(CoarsePoint2d_compare(p, p));
  }
}

static void test6(void)
{
  /* Unequal points */

  assert(!CoarsePoint2d_compare((CoarsePoint2d){1, 2}, (CoarsePoint2d){3, 2}));
  assert(!CoarsePoint2d_compare((CoarsePoint2d){1, 2}, (CoarsePoint2d){1, 3}));
}

static void test7(void)
{
  /* CoarsePoint2d round trip */

  for (unsigned v = 0; v < 256; ++v)
  {
    CoarsePoint2d const input = {(CoarseCoord)v, (CoarseCoord)(255 - v)};
    unsigned char data[2];
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    CoarsePoint2d_write(input, &writer);
    assert(writer_destroy(&writer) == sizeof(data));
    assert(data[0] == v);
    assert(data[1] == 255 - v);
    Reader reader;
    assert(reader_mem_init(&reader, data, sizeof(data)));
    CoarsePoint2d output;
    assert(CoarsePoint2d_read(&output, &reader));
    assert(output.x == input.x);
    assert(output.y == input.y);

    reader_destroy(&reader);
  }
}

static void test8(void)
{
  /* CoarsePoint2d truncation */

  unsigned char const data[2] = {0};
  for (size_t length = 0; length < sizeof(data); ++length)
  {
    Reader reader;
    assert(reader_mem_init(&reader, data, length));
    CoarsePoint2d p;
    assert(!CoarsePoint2d_read(&p, &reader));
    reader_destroy(&reader);
  }
}

static void test9(void)
{
  /* CoarsePoint3d round trip */

  for (unsigned v = 0; v < 256; ++v)
  {
    CoarsePoint3d const input = {.x = (CoarseCoord)v, .y = (CoarseCoord)(255 - v), .z = (CoarseCoord)(v ^ 85)};
    unsigned char data[3];
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    CoarsePoint3d_write(input, &writer);
    assert(writer_destroy(&writer) == sizeof(data));
    assert(data[0] == v);
    assert(data[1] == 255 - v);
    assert(data[2] == (v ^ 85));
    Reader reader;
    assert(reader_mem_init(&reader, data, sizeof(data)));
    CoarsePoint3d output;
    assert(CoarsePoint3d_read(&output, &reader));
    assert(output.x == input.x);
    assert(output.y == input.y);
    assert(output.z == input.z);
    reader_destroy(&reader);
  }
}

static void test10(void)
{
  /* CoarsePoint3d truncation */

  unsigned char const data[3] = {0};
  for (size_t length = 0; length < sizeof(data); ++length)
  {
    Reader reader;
    assert(reader_mem_init(&reader, data, length));
    CoarsePoint3d p;
    assert(!CoarsePoint3d_read(&p, &reader));
    reader_destroy(&reader);
  }
}

static void test11(void)
{
  /* FinePoint3d round trip */

  for (unsigned v = 0; v < 256; ++v)
  {
    FinePoint3d const input = {
      .x = UINT32_C(0x01020304) ^ (uint32_t)v,
      .y = UINT32_C(0x89ABCDEF) ^ ((uint32_t)v << 16),
      .z = UINT32_MAX - UINT32_C(0x01010101) * v,
    };
    unsigned char data[12];
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    FinePoint3d_write(input, &writer);
    assert(writer_destroy(&writer) == sizeof(data));
    for (size_t axis = 0; axis < 3; ++axis)
    {
      uint32_t const value = axis == 0 ? input.x : (axis == 1 ? input.y : input.z);
      for (size_t byte = 0; byte < 4; ++byte)
      {
        assert(data[axis * 4 + byte] == ((value >> (8 * byte)) & 255));
      }
    }
    Reader reader;
    assert(reader_mem_init(&reader, data, sizeof(data)));
    FinePoint3d output;
    assert(FinePoint3d_read(&output, &reader));
    assert(output.x == input.x);
    assert(output.y == input.y);
    assert(output.z == input.z);
    reader_destroy(&reader);
  }
}

static void test12(void)
{
  /* FinePoint3d truncation */

  unsigned char const data[12] = {0};
  for (size_t length = 0; length < sizeof(data); ++length)
  {
    Reader reader;
    assert(reader_mem_init(&reader, data, length));
    FinePoint3d p;
    assert(!FinePoint3d_read(&p, &reader));
    reader_destroy(&reader);
  }
}

void CoarseCoord_tests(void)
{
  /* CoarseCoord */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Scalar expansion", test1 },
    { "Scalar quantisation", test2 },
    { "Vector expansion", test3 },
    { "Vector quantisation", test4 },
    { "Equal points", test5 },
    { "Unequal points", test6 },
    { "CoarsePoint2d round trip", test7 },
    { "CoarsePoint2d truncation", test8 },
    { "CoarsePoint3d round trip", test9 },
    { "CoarsePoint3d truncation", test10 },
    { "FinePoint3d round trip", test11 },
    { "FinePoint3d truncation", test12 },
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
