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

enum { HeaderSize = 48, Capacity = 512 };

/* A minimal valid block, with positive duration and speed, no clipping,
 * and an empty string. All other header fields are zero. */
static unsigned char const empty_block[HeaderSize + 1] = {
  [8] = 1, [16] = 1, [32] = 255, [HeaderSize] = 255,
};

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

static SFError read_block(Text *text, unsigned char const *data, size_t size)
{
  Reader reader;
  assert(reader_mem_init(&reader, data, size));
  SFError const err = text_read_block(text, &reader);
  reader_destroy(&reader);
  return err;
}

static size_t write_block(Text const *text, unsigned char (*data)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *data, sizeof(*data)));
  text_write_block(text, &writer);
  assert(!writer_ferror(&writer));
  long const size = writer_destroy(&writer);
  assert(size >= HeaderSize + 1);
  assert(size <= Capacity);
  return (size_t)size;
}

static void check_field(size_t offset, int32_t value, SFErrorType expected)
{
  unsigned char data[sizeof(empty_block)];
  memcpy(data, empty_block, sizeof(data));
  Writer writer;
  assert(writer_mem_init(&writer, data + offset, 4));
  writer_fwrite_int32(value, &writer);
  assert(writer_destroy(&writer) == 4);

  Text text;
  text_init(&text);
  assert(read_block(&text, data, sizeof(data)).type == expected);
  text_destroy(&text);
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
    assert(!SFError_fail(read_block(&loaded, data, size)));
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
  /* Header fields */

  Text text;
  text_init(&text);
  text_set_params(&text, &params);
  unsigned char data[Capacity];
  size_t const size = write_block(&text, &data);
  static const struct { size_t offset; int32_t value; } fields[] = {
    {.offset = 8, .value = INT32_MAX}, {.offset = 12, .value = INT32_MAX},
    {.offset = 16, .value = INT32_MAX}, {.offset = 20, .value = Text_NumColumns - 1},
    {.offset = 24, .value = Text_ScreenHeight - 1}, {.offset = 32, .value = Text_NumRows},
    {.offset = 36, .value = 1}, {.offset = 40, .value = CursorType_Block},
    {.offset = 44, .value = 255},
  };
  Reader reader;
  assert(reader_mem_init(&reader, data, size));
  for (size_t i = 0; i < ARRAY_SIZE(fields); ++i)
  {
    assert(reader_fseek(&reader, (long)fields[i].offset, SEEK_SET) == 0);
    int32_t value;
    assert(reader_fread_int32(&value, &reader));
    assert(value == fields[i].value);
  }
  reader_destroy(&reader);
  text_destroy(&text);
}

static void test14(void)
{
  /* Encoded alphabet */

  char input[79];
  for (size_t i = 0; i < sizeof(input) - 1; ++i)
  {
    input[i] = (char)('a' + i % 26);
  }
  input[sizeof(input) - 1] = '\0';
  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, input)));
  unsigned char data[Capacity];
  write_block(&text, &data);
  for (size_t i = 0; i < sizeof(input) - 1; ++i)
  {
    assert(data[HeaderSize + i] == 10 + i % 26);
  }
  assert(data[HeaderSize + sizeof(input) - 1] == 255);
  text_destroy(&text);
}

static void test15(void)
{
  /* Offset alignment */

  char input[] = "ABCDEFG";
  for (size_t length = 0; length < sizeof(input); ++length)
  {
    char const saved = input[length];
    input[length] = '\0';
    Text text;
    text_init(&text);
    assert(!SFError_fail(text_set_string(&text, input)));
    unsigned char data[8];
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    int offset = 100;
    text_write_offset(&text, &writer, &offset);
    int const block_size = HeaderSize + (int)((length + 4) / 4 * 4);
    assert(offset == 100 + block_size);
    text_write_offset(&text, &writer, &offset);
    assert(offset == 100 + 2 * block_size);
    assert(writer_destroy(&writer) == sizeof(data));
    Reader reader;
    assert(reader_mem_init(&reader, data, sizeof(data)));
    int32_t value;
    assert(reader_fread_int32(&value, &reader));
    assert(value == 100);
    assert(reader_fread_int32(&value, &reader));
    assert(value == 100 + block_size);
    reader_destroy(&reader);
    text_destroy(&text);
    input[length] = saved;
  }
}

static void test16(void)
{
  /* Truncated block */

  for (size_t length = 0; length < sizeof(empty_block); ++length)
  {
    Text text;
    text_init(&text);
    assert(read_block(&text, empty_block, length).type == SFErrorType_ReadFail);
    text_destroy(&text);
  }
}

static void test17(void)
{
  /* Zero duration */

  unsigned char data[sizeof(empty_block)];
  memcpy(data, empty_block, sizeof(data));
  data[8] = 0;
  Text text;
  text_init(&text);
  assert(!SFError_fail(read_block(&text, data, sizeof(data))));
  TextParams actual;
  text_get_params(&text, &actual);
  assert(actual.duration == INT32_MAX);
  text_destroy(&text);
}

static void test18(void)
{
  /* Invalid duration */

  check_field(8, -1, SFErrorType_BadTextDuration);
}

static void test19(void)
{
  /* Invalid delay */

  check_field(12, -1, SFErrorType_BadTextDelay);
}

static void test20(void)
{
  /* Invalid speed */

  check_field(16, -1, SFErrorType_BadTextSpeed);
}

static void test21(void)
{
  /* Invalid x position */

  check_field(20, -1, SFErrorType_BadTextXPos);
  check_field(20, Text_NumColumns, SFErrorType_BadTextXPos);
}

static void test22(void)
{
  /* Invalid y position */

  check_field(24, -1, SFErrorType_BadTextYPos);
  check_field(24, Text_ScreenHeight, SFErrorType_BadTextYPos);
}

static void test23(void)
{
  /* Invalid clipping */

  check_field(32, -1, SFErrorType_BadTextYClip);
  check_field(32, Text_NumRows + 1, SFErrorType_BadTextYClip);
  check_field(32, Text_NoYClip - 1, SFErrorType_BadTextYClip);
  check_field(32, Text_NoYClip + 1, SFErrorType_BadTextYClip);
}

static void test24(void)
{
  /* Invalid repeat */

  check_field(36, -1, SFErrorType_BadTextRepeat);
  check_field(36, 2, SFErrorType_BadTextRepeat);
}

static void test25(void)
{
  /* Invalid cursor */

  check_field(40, -1, SFErrorType_BadTextCursorType);
  check_field(40, CursorType_Count, SFErrorType_BadTextCursorType);
}

static void test26(void)
{
  /* Invalid colour */

  check_field(44, -1, SFErrorType_BadTextColour);
  check_field(44, 256, SFErrorType_BadTextColour);
  check_field(44, INT32_MAX, SFErrorType_BadTextColour);
}

static void test27(void)
{
  /* Unknown character codes */

  static unsigned char const codes[] = {36, 37, 42, 45, 46, 50, 51, 52, 53, 54, 56, 62, 63, 66, 253};
  for (size_t i = 0; i < ARRAY_SIZE(codes); ++i)
  {
    unsigned char data[HeaderSize + 2];
    memcpy(data, empty_block, sizeof(empty_block));
    data[HeaderSize] = codes[i];
    data[HeaderSize + 1] = 255;
    Text text;
    text_init(&text);
    assert(!SFError_fail(read_block(&text, data, sizeof(data))));
    assert(strcmp(text_get_string(&text), "#") == 0);
    text_destroy(&text);
  }
}

static void test28(void)
{
  /* Read replacement */

  Text text;
  text_init(&text);
  assert(!SFError_fail(text_set_string(&text, "OLD ALLOCATION")));
  assert(!SFError_fail(read_block(&text, empty_block, sizeof(empty_block))));
  assert(strcmp(text_get_string(&text), "") == 0);
  text_destroy(&text);
}

#ifdef FORTIFY
static void test29(void)
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
static void test30(void)
{
  /* Read allocation failure */

  unsigned char data[HeaderSize + 2];
  memcpy(data, empty_block, sizeof(empty_block));
  data[HeaderSize] = 10;
  data[HeaderSize + 1] = 255;
  Reader reader;
  assert(reader_mem_init(&reader, data, sizeof(data)));
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
    { "Header fields", test13 },
    { "Encoded alphabet", test14 },
    { "Offset alignment", test15 },
    { "Truncated block", test16 },
    { "Zero duration", test17 },
    { "Invalid duration", test18 },
    { "Invalid delay", test19 },
    { "Invalid speed", test20 },
    { "Invalid x position", test21 },
    { "Invalid y position", test22 },
    { "Invalid clipping", test23 },
    { "Invalid repeat", test24 },
    { "Invalid cursor", test25 },
    { "Invalid colour", test26 },
    { "Unknown character codes", test27 },
    { "Read replacement", test28 },
#ifdef FORTIFY
    { "String allocation failure", test29 },
#endif
#ifdef FORTIFY
    { "Read allocation failure", test30 },
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
