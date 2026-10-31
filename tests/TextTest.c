/* SFEditor test: text strings and binary blocks.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Writer.h"
#include "WriterMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "../Text.h"
#include "../TextData.h"

enum { Capacity = 256 };

static TextParams const params = {
  .duration = INT32_MAX, .delay = INT32_MAX, .speed = INT32_MAX,
  .y_pos = Text_ScreenHeight - 1, .x_pos = Text_NumColumns - 1,
  .y_clip = Text_NumRows, .colour = 255, .repeat = true,
  .cursor_type = CursorType_Block,
};

static void check_params(TextParams const *actual, TextParams const *expected)
{
  assert(actual->duration == expected->duration);
  assert(actual->delay == expected->delay);
  assert(actual->speed == expected->speed);
  assert(actual->y_pos == expected->y_pos);
  assert(actual->x_pos == expected->x_pos);
  assert(actual->y_clip == expected->y_clip);
  assert(actual->colour == expected->colour);
  assert(actual->repeat == expected->repeat);
  assert(actual->cursor_type == expected->cursor_type);
}

static SFError read_data(Text *value,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = text_read_block(value, &reader);
  reader_destroy(&reader);
  return err;
}

static size_t write_block(Text const *value, unsigned char (*buffer)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  text_write_block(value, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise any space skipped at the end, without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFErrorType const expected_errors[] = {
  SFErrorType_BadTextDuration,
  SFErrorType_BadTextDelay,
  SFErrorType_BadTextSpeed,
  SFErrorType_BadTextXPos,
  SFErrorType_BadTextYPos,
  SFErrorType_BadTextYClip,
  SFErrorType_BadTextRepeat,
  SFErrorType_BadTextCursorType,
  SFErrorType_BadTextColour,
  SFErrorType_ReadFail,
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
  /* Empty initial string */

  Text text;
  text_init(&text);
  assert(strcmp(text_get_string(&text), "") == 0);
  text_destroy(&text);
}

static void test2(void)
{
  /* Default parameters */

  Text text;
  text_init(&text);
  TextParams actual;
  text_get_params(&text, &actual);
  TextParams const expected = {
    .duration = 5000, .delay = 0, .speed = 3, .y_pos = 0, .x_pos = 0,
    .y_clip = Text_NoYClip, .colour = 255, .repeat = false,
    .cursor_type = CursorType_None,
  };
  check_params(&actual, &expected);
  text_destroy(&text);
}

static void test3(void)
{
  /* Alphabet normalisation */

  char input[261];
  for (size_t i = 0; i < sizeof(input) - 1; ++i)
  {
    input[i] = (char)((i % 2 ? 'a' : 'A') + i % 26);
  }
  input[sizeof(input) - 1] = '\0';
  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, input)));
  char const *const actual = text_get_string(&text);
  for (size_t i = 0; i < sizeof(input) - 1; ++i)
  {
    assert(actual[i] == (char)('A' + i % 26));
  }
  assert(actual[sizeof(input) - 1] == '\0');
  text_destroy(&text);
}

static void test4(void)
{
  /* Decimal digits */

  char input[101];
  for (size_t i = 0; i < sizeof(input) - 1; ++i)
  {
    input[i] = (char)('0' + i % 10);
  }
  input[sizeof(input) - 1] = '\0';
  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, input)));
  assert(strcmp(text_get_string(&text), input) == 0);
  text_destroy(&text);
}

static void test5(void)
{
  /* Supported punctuation */

  static char const input[] = ".\xA3-/> <%'?[](),:!\n";
  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, input)));
  assert(strcmp(text_get_string(&text), input) == 0);
  text_destroy(&text);
}

static void test6(void)
{
  /* Unsupported characters */

  static char const input[] = "@#$&*+;=^_`{|}~\t\r";
  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, input)));
  char const *const actual = text_get_string(&text);
  for (size_t i = 0; i < sizeof(input) - 1; ++i)
  {
    assert(actual[i] == ' ');
  }
  assert(actual[sizeof(input) - 1] == '\0');
  text_destroy(&text);
}

static void test7(void)
{
  /* String replacement */

  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, "A LONGER ORIGINAL STRING")));
  assert(!SFError_fail(text_set_string(&text, "NEW")));
  assert(strcmp(text_get_string(&text), "NEW") == 0);
  text_destroy(&text);
}

static void test8(void)
{
  /* Clear string */

  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, "ORIGINAL")));
  assert(!SFError_fail(text_set_string(&text, "")));
  assert(strcmp(text_get_string(&text), "") == 0);
  text_destroy(&text);
}

static void test9(void)
{
  /* Set parameters */

  Text text;
  text_init(&text);
  text_set_params(&text, &params);
  TextParams actual;
  text_get_params(&text, &actual);
  check_params(&actual, &params);
  text_destroy(&text);
}

static void test10(void)
{
  /* String preserves parameters */

  Text text;
  text_init(&text);
  text_set_params(&text, &params);
  assert(!SFError_fail(text_set_string(&text, "REPLACEMENT")));
  TextParams actual;
  text_get_params(&text, &actual);
  check_params(&actual, &params);
  text_destroy(&text);
}

static void test11(void)
{
  /* Parameters preserve string */

  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, "UNCHANGED")));
  text_set_params(&text, &params);
  assert(strcmp(text_get_string(&text), "UNCHANGED") == 0);
  text_destroy(&text);
}

static void test12(void)
{
  /* Block round trip */

  char input[132];
  for (size_t i = 0; i < sizeof(input) - 1; ++i)
  {
    input[i] = (char)('A' + i % 26);
  }
  input[sizeof(input) - 1] = '\0';
  for (size_t length = 0; length < sizeof(input); ++length)
  {
    char const saved = input[length];
    input[length] = '\0';
    Text source, loaded;
    text_init(&source);
    text_init(&loaded);
    text_set_params(&source, &params);
    assert(!SFError_fail(text_set_string(&source, input)));
    unsigned char data[Capacity];
    size_t const size = write_block(&source, &data);
    assert(!SFError_fail(read_data(&loaded, &data, size)));
    assert(strcmp(text_get_string(&loaded), input) == 0);
    TextParams actual;
    text_get_params(&loaded, &actual);
    check_params(&actual, &params);
    text_destroy(&loaded);
    text_destroy(&source);
    input[length] = saved;
  }
}

static void test13(void)
{
  /* Offset progression */

  char input[] = "ABCDEFG";
  for (size_t length = 0; length < sizeof(input); ++length)
  {
    char const saved = input[length];
    input[length] = '\0';
    Text text;
    text_init(&text);
    assert(!SFError_fail(text_set_string(&text, input)));
    unsigned char block[Capacity], data[Capacity];
    size_t const block_size = write_block(&text, &block);
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    int offset = 100;
    text_write_offset(&text, &writer, &offset);
    assert(offset == 100 + (int)block_size);
    text_write_offset(&text, &writer, &offset);
    assert(offset == 100 + 2 * (int)block_size);
    assert(writer_destroy(&writer) >= 0);
    text_destroy(&text);
    input[length] = saved;
  }
}

static void test14(void)
{
  /* Truncated block */

  Text source;
  text_init(&source);
  assert(!SFError_fail(text_set_string(&source, "ABC")));
  unsigned char buffer[Capacity];
  size_t const size = write_block(&source, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    Text loaded;
    text_init(&loaded);
    SFError const err = read_data(&loaded, &buffer, length);
    if (length == 0)
    {
      assert(SFError_fail(err));
    }
    if (!SFError_fail(err))
    {
      assert(strcmp(text_get_string(&loaded), "ABC") == 0);
    }
    text_destroy(&loaded);
  }
  text_destroy(&source);
}

static void test15(void)
{
  /* Read replacement */

  Text source, text;
  text_init(&source);
  text_init(&text);
  unsigned char buffer[Capacity];
  size_t const size = write_block(&source, &buffer);
  assert(!SFError_fail(text_set_string(&text, "OLD ALLOCATION")));
  assert(!SFError_fail(read_data(&text, &buffer, size)));
  assert(strcmp(text_get_string(&text), "") == 0);
  text_destroy(&text);
  text_destroy(&source);
}

static void test16(void)
{
  /* Corrupt text data */

  Text source;
  text_init(&source);
  TextParams settings;
  text_get_params(&source, &settings);
  settings.duration = 1;
  text_set_params(&source, &settings);
  assert(!SFError_fail(text_set_string(&source, "ABC")));
  unsigned char buffer[Capacity];
  size_t const size = write_block(&source, &buffer);
  text_destroy(&source);
  bool seen[ErrorCount] = {false};
  for (size_t offset = 0; offset < size; ++offset)
  {
    unsigned char const saved = buffer[offset];
    for (unsigned byte = 0; byte < 256; ++byte)
    {
      buffer[offset] = (unsigned char)byte;
      Text loaded;
            text_init(&loaded);
      note_error(read_data(&loaded, &buffer, size).type, &seen);
      text_destroy(&loaded);
    }
    buffer[offset] = saved;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    assert(seen[i]);
  }
}

#ifdef FORTIFY
static void test17(void)
{
  /* String allocation failure */

  Text text;
  text_init(&text);
  Fortify_SetNumAllocationsLimit(0);
  SFError const err = text_set_string(&text, "NEW ALLOCATION");
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(err.type == SFErrorType_NoMem);
  text_destroy(&text);
}

#endif

#ifdef FORTIFY
static void test18(void)
{
  /* Read allocation failure */

  Text source;
  text_init(&source);
  assert(!SFError_fail(text_set_string(&source, "ABC")));
  unsigned char buffer[Capacity];
  size_t const size = write_block(&source, &buffer);
  text_destroy(&source);
  Reader reader;
  assert(reader_mem_init(&reader, buffer, size));
  Text text;
  text_init(&text);
  Fortify_SetNumAllocationsLimit(0);
  SFError const err = text_read_block(&text, &reader);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(err.type == SFErrorType_NoMem);
  text_destroy(&text);
  reader_destroy(&reader);
}

#endif

void Text_tests(void)
{
  /* Text */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Empty initial string", test1 },
    { "Default parameters", test2 },
    { "Alphabet normalisation", test3 },
    { "Decimal digits", test4 },
    { "Supported punctuation", test5 },
    { "Unsupported characters", test6 },
    { "String replacement", test7 },
    { "Clear string", test8 },
    { "Set parameters", test9 },
    { "String preserves parameters", test10 },
    { "Parameters preserve string", test11 },
    { "Block round trip", test12 },
    { "Offset progression", test13 },
    { "Truncated block", test14 },
    { "Read replacement", test15 },
    { "Corrupt text data", test16 },
#ifdef FORTIFY
    { "String allocation failure", test17 },
#endif
#ifdef FORTIFY
    { "Read allocation failure", test18 },
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
