/* SFEditor test: mission briefing.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
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
#include "../Briefing.h"
#include "../BriefingData.h"

/* Enough for all seven short text blocks, including their headers. */
enum { Capacity = 512 };

static size_t write_data(BriefingData *briefing,
  unsigned char (*buffer)[Capacity])
{
  memset(*buffer, 0, sizeof(*buffer));
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  briefing_write_texts(briefing, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length >= 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise trailing space skipped by the writer. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(BriefingData *briefing,
  long C23_CONST (*offsets)[BriefingMax], size_t count,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  Reader reader;
  assert(length <= sizeof(*buffer));
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = briefing_read_texts(briefing, *offsets, count, &reader);
  reader_destroy(&reader);
  return err;
}

static void check_texts(BriefingData const *briefing, size_t count)
{
  assert(briefing_get_text_count(briefing) == count);
  for (size_t i = 0; i < count; ++i)
  {
    char const expected[] = {(char)('A' + i), '\0'};
    assert(strcmp(briefing_get_text(briefing, i), expected) == 0);
  }
}

/* Record each block's offset by writing the preceding prefix. */
static size_t make_data(BriefingData *briefing, size_t count,
  long (*offsets)[BriefingMax], unsigned char (*buffer)[Capacity])
{
  briefing_init(briefing);
  for (size_t i = 0; i < count; ++i)
  {
    (*offsets)[i] = (long)write_data(briefing, buffer);
    char const string[] = {(char)('A' + i), '\0'};
    assert(!SFError_fail(briefing_add_text(briefing, string)));
  }
  return write_data(briefing, buffer);
}

static void test1(void)
{
  /* Initial text count */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(briefing_get_text_count(&briefing) == 0);
  briefing_destroy(&briefing);
}

static void test2(void)
{
  /* Missing title */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(strcmp(briefing_get_title(&briefing), "") == 0);
  briefing_destroy(&briefing);
}

static void test3(void)
{
  /* Added text order */

  BriefingData briefing;
  briefing_init(&briefing);
  for (size_t i = 0; i < BriefingMax; ++i)
  {
    char string[] = {(char)('A' + i), '\0'};
    assert(!SFError_fail(briefing_add_text(&briefing, string)));
    string[0] = 'Z';
    check_texts(&briefing, i + 1);
  }
  briefing_destroy(&briefing);
}

static void test4(void)
{
  /* Empty text */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(!SFError_fail(briefing_add_text(&briefing, "")));
  assert(briefing_get_text_count(&briefing) == 1);
  assert(strcmp(briefing_get_text(&briefing, 0), "") == 0);
  briefing_destroy(&briefing);
}

static void test5(void)
{
  /* Title prefix */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(!SFError_fail(briefing_add_text(&briefing, "MISSION: ALPHA")));
  assert(strcmp(briefing_get_title(&briefing), "ALPHA") == 0);
  briefing_destroy(&briefing);
}

static void test6(void)
{
  /* Title leading spaces */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(!SFError_fail(briefing_add_text(&briefing, "MISSION:   ALPHA")));
  assert(strcmp(briefing_get_title(&briefing), "ALPHA") == 0);
  briefing_destroy(&briefing);
}

static void test7(void)
{
  /* Title without prefix */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(!SFError_fail(briefing_add_text(&briefing, "ALPHA")));
  assert(strcmp(briefing_get_title(&briefing), "") == 0);
  briefing_destroy(&briefing);
}

static void test8(void)
{
  /* Empty title suffix */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(!SFError_fail(briefing_add_text(&briefing, "MISSION:   ")));
  assert(strcmp(briefing_get_title(&briefing), "") == 0);
  briefing_destroy(&briefing);
}

static void test9(void)
{
  /* Title embedded colon */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(!SFError_fail(briefing_add_text(&briefing, "MISSION: ALPHA:BETA")));
  assert(strcmp(briefing_get_title(&briefing), "ALPHA:BETA") == 0);
  briefing_destroy(&briefing);
}

static void test10(void)
{
  /* Title trailing spaces */

  BriefingData briefing;
  briefing_init(&briefing);
  assert(!SFError_fail(briefing_add_text(&briefing, "MISSION: ALPHA  ")));
  assert(strcmp(briefing_get_title(&briefing), "ALPHA  ") == 0);
  briefing_destroy(&briefing);
}

static void test11(void)
{
  /* Text count limit */

  BriefingData briefing;
  briefing_init(&briefing);
  for (size_t i = 0; i < BriefingMax; ++i)
  {
    char const string[] = {(char)('A' + i), '\0'};
    assert(!SFError_fail(briefing_add_text(&briefing, string)));
  }
  assert(briefing_add_text(&briefing, "EXCESS").type ==
         SFErrorType_TooManyBriefingStrings);
  check_texts(&briefing, BriefingMax);
  briefing_destroy(&briefing);
}

static void test12(void)
{
  /* Text width limit */

  char string[128];
  bool rejected = false;
  for (size_t width = 0; width + 1 < sizeof(string); ++width)
  {
    memset(string, 'A', width);
    string[width] = '\0';
    BriefingData briefing;
    briefing_init(&briefing);
    assert(!SFError_fail(briefing_add_text(&briefing, "TITLE: TEST")));
    SFError const err = briefing_add_text(&briefing, string);
    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_TooManyBriefingColumns);
      assert(briefing_get_text_count(&briefing) == 1);
      assert(strcmp(briefing_get_text(&briefing, 0), "TITLE: TEST") == 0);
      assert(!SFError_fail(briefing_add_text(&briefing, "SHORT")));
      rejected = true;
    }
    else
    {
      assert(strcmp(briefing_get_text(&briefing, 1), string) == 0);
    }
    briefing_destroy(&briefing);
    if (rejected) { break; }
  }
  assert(rejected);
}

static void test13(void)
{
  /* Text line limit */

  char string[64];
  bool rejected = false;
  for (size_t lines = 0; lines + 1 < sizeof(string); ++lines)
  {
    memset(string, '\n', lines);
    string[lines] = '\0';
    BriefingData briefing;
    briefing_init(&briefing);
    assert(!SFError_fail(briefing_add_text(&briefing, "TITLE: TEST")));
    SFError const err = briefing_add_text(&briefing, string);
    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_TooManyBriefingLines);
      assert(briefing_get_text_count(&briefing) == 1);
      assert(strcmp(briefing_get_text(&briefing, 0), "TITLE: TEST") == 0);
      assert(!SFError_fail(briefing_add_text(&briefing, "SHORT")));
      rejected = true;
    }
    else
    {
      assert(strcmp(briefing_get_text(&briefing, 1), string) == 0);
    }
    briefing_destroy(&briefing);
    if (rejected) { break; }
  }
  assert(rejected);
}

static void test14(void)
{
  /* Briefing round trip */

  for (size_t count = BriefingMin; count <= BriefingMax; ++count)
  {
    BriefingData source, loaded;
    unsigned char buffer[Capacity];
    long offsets[BriefingMax];
    size_t const length = make_data(&source, count, &offsets, &buffer);
    briefing_init(&loaded);
    assert(!SFError_fail(read_data(&loaded, &offsets, count, &buffer, length)));
    check_texts(&loaded, count);
    briefing_destroy(&loaded);
    briefing_destroy(&source);
  }
}

static void test15(void)
{
  /* Briefing truncation */

  BriefingData source;
  unsigned char buffer[Capacity];
  long offsets[BriefingMax];
  size_t const count = 2;
  size_t const size = make_data(&source, count, &offsets, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    BriefingData loaded;
    briefing_init(&loaded);
    SFError const err = read_data(&loaded, &offsets, count, &buffer, length);
    if (length == 0) { assert(SFError_fail(err)); }
    if (!SFError_fail(err)) { check_texts(&loaded, count); }
    briefing_destroy(&loaded);
  }
  briefing_destroy(&source);
}

static void test16(void)
{
  /* Invalid text offset */

  BriefingData source, loaded;
  unsigned char buffer[Capacity];
  long offsets[BriefingMax];
  size_t const length = make_data(&source, 2, &offsets, &buffer);
  offsets[1] = -1;
  briefing_init(&loaded);
  assert(read_data(&loaded, &offsets, 2, &buffer, length).type == SFErrorType_BadSeek);
  briefing_destroy(&loaded);
  briefing_destroy(&source);
}

static void test17(void)
{
  /* Text offset extent */

  for (size_t count = 0; count <= BriefingMax; ++count)
  {
    BriefingData source;
    unsigned char buffer[Capacity];
    long offsets[BriefingMax];
    size_t const length = make_data(&source, count, &offsets, &buffer);
    Writer writer;
    assert(writer_mem_init(&writer, buffer, sizeof(buffer)));
    int const start = 37;
    assert(briefing_write_text_offsets(&source, &writer, start) == start + (int)length);
    assert(!writer_ferror(&writer));
    assert(writer_destroy(&writer) >= 0);
    briefing_destroy(&source);
  }
}

static void test18(void)
{
  /* Text output failure */

  BriefingData source;
  unsigned char buffer[Capacity];
  long offsets[BriefingMax];
  make_data(&source, 2, &offsets, &buffer);
  for (size_t capacity = 0; capacity <= (size_t)offsets[1]; ++capacity)
  {
    Writer writer;
    assert(writer_mem_init(&writer, buffer, capacity));
    briefing_write_texts(&source, &writer);
    assert(writer_ferror(&writer));
    writer_destroy(&writer);
  }
  briefing_destroy(&source);
}

static void test19(void)
{
  /* Offset output failure */

  BriefingData source;
  unsigned char buffer[Capacity];
  long offsets[BriefingMax];
  make_data(&source, 2, &offsets, &buffer);
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));
  briefing_write_text_offsets(&source, &writer, 0);
  assert(writer_ferror(&writer));
  writer_destroy(&writer);
  briefing_destroy(&source);
}

#ifdef FORTIFY
static void test20(void)
{
  /* Add allocation failure */

  for (unsigned long limit = 0; limit < 2; ++limit)
  {
    BriefingData briefing;
    briefing_init(&briefing);
    assert(!SFError_fail(briefing_add_text(&briefing, "A")));
    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = briefing_add_text(&briefing, "SECOND TEXT");
    Fortify_SetNumAllocationsLimit(ULONG_MAX);
    assert(err.type == SFErrorType_NoMem);
    check_texts(&briefing, 1);
    assert(!SFError_fail(briefing_add_text(&briefing, "B")));
    check_texts(&briefing, 2);
    briefing_destroy(&briefing);
  }
}

#endif

#ifdef FORTIFY
static void test21(void)
{
  /* Read allocation failure */

  BriefingData source, loaded;
  unsigned char buffer[Capacity];
  long offsets[BriefingMax];
  size_t const length = make_data(&source, 2, &offsets, &buffer);
  for (unsigned long limit = 0; limit < 3; ++limit)
  {
    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));
    briefing_init(&loaded);
    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = briefing_read_texts(&loaded, offsets, 2, &reader);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);
    assert(err.type == SFErrorType_NoMem);
    briefing_destroy(&loaded);
    reader_destroy(&reader);
  }
  briefing_destroy(&source);
}

#endif

static void test22(void)
{
  /* Text offset order */

  BriefingData source, loaded;
  unsigned char buffer[Capacity];
  long offsets[BriefingMax];
  size_t const length = make_data(&source, BriefingMax, &offsets, &buffer);
  for (size_t i = 0; i < BriefingMax / 2; ++i)
  {
    long const saved = offsets[i];
    offsets[i] = offsets[BriefingMax - 1 - i];
    offsets[BriefingMax - 1 - i] = saved;
  }
  briefing_init(&loaded);
  assert(!SFError_fail(read_data(&loaded, &offsets, BriefingMax, &buffer, length)));
  assert(briefing_get_text_count(&loaded) == BriefingMax);
  for (size_t i = 0; i < BriefingMax; ++i)
  {
    char const expected[] = {(char)('A' + BriefingMax - 1 - i), '\0'};
    assert(strcmp(briefing_get_text(&loaded, i), expected) == 0);
  }
  briefing_destroy(&loaded);
  briefing_destroy(&source);
}

static void test23(void)
{
  /* Multiline round trip */

  BriefingData source, loaded;
  briefing_init(&source);
  char const string[] = "TITLE: TEST\nSECOND LINE\n";
  assert(!SFError_fail(briefing_add_text(&source, string)));
  unsigned char buffer[Capacity];
  long offsets[BriefingMax] = {0};
  size_t const length = write_data(&source, &buffer);
  briefing_init(&loaded);
  assert(!SFError_fail(read_data(&loaded, &offsets, 1, &buffer, length)));
  assert(briefing_get_text_count(&loaded) == 1);
  assert(strcmp(briefing_get_text(&loaded, 0), string) == 0);
  assert(strcmp(briefing_get_title(&loaded), "TEST\nSECOND LINE\n") == 0);
  briefing_destroy(&loaded);
  briefing_destroy(&source);
}

void Briefing_tests(void)
{
  /* Briefing */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial text count", test1 },
    { "Missing title", test2 },
    { "Added text order", test3 },
    { "Empty text", test4 },
    { "Title prefix", test5 },
    { "Title leading spaces", test6 },
    { "Title without prefix", test7 },
    { "Empty title suffix", test8 },
    { "Title embedded colon", test9 },
    { "Title trailing spaces", test10 },
    { "Text count limit", test11 },
    { "Text width limit", test12 },
    { "Text line limit", test13 },
    { "Briefing round trip", test14 },
    { "Briefing truncation", test15 },
    { "Invalid text offset", test16 },
    { "Text offset extent", test17 },
    { "Text output failure", test18 },
    { "Offset output failure", test19 },
    { "Text offset order", test22 },
    { "Multiline round trip", test23 },
#ifdef FORTIFY
    { "Add allocation failure", test20 },
#endif
#ifdef FORTIFY
    { "Read allocation failure", test21 },
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
