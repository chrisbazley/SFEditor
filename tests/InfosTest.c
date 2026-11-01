/* SFEditor test: target information.
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
#include "Tests.h"
#include "../SFError.h"
#include "Writer.h"
#include "WriterMem.h"
#include "../Infos.h"
#include "../InfosData.h"
#include "../Map.h"
#include "../Text.h"
#include "../TextData.h"

enum
{
  Capacity = 128,
  TextCapacity = 1024,
  OffsetCount = TargetInfoMax * TargetInfoTextIndex_Count
};

typedef enum {
  InfoField_AllMustMatch,
  InfoField_Position,
  InfoField_Type,
  InfoField_Details,
} InfoField;

static void check_fields(TargetInfo const *info, MapPoint pos,
  char const *type, char const *details, InfoField except)
{
  if (except != InfoField_Position)
  {
    MapPoint const actual = target_info_get_pos(info);
    assert(actual.x == pos.x);
    assert(actual.y == pos.y);
  }

  if (except != InfoField_Type)
  {
    assert(strcmp(target_info_get_text(info, TargetInfoTextIndex_Type), type) == 0);
  }

  if (except != InfoField_Details)
  {
    assert(strcmp(target_info_get_text(info, TargetInfoTextIndex_Details), details) == 0);
  }
}

static MapPoint position(size_t index)
{
  return (MapPoint){(MapCoord)(index % 3) * 16, (MapCoord)(index / 3) * 16};
}

static void populate(TargetInfosData *infos, size_t count)
{
  target_infos_init(infos);

  for (size_t index = 0; index < count; ++index)
  {
    size_t added;
    assert(!SFError_fail(target_infos_add(infos, position(index), &added)));
    assert(added == index);

    _Optional TargetInfo *const info = target_info_from_index(infos, added);
    assert(info);

    for (TargetInfoTextIndex field = TargetInfoTextIndex_First;
         field < TargetInfoTextIndex_Count; ++field)
    {
      char const string[] = {(char)('A' + index), ':', (char)('0' + field), '\0'};
      assert(!SFError_fail(target_info_set_text(&*info, field, string)));
    }
  }
}

static void check_pattern(TargetInfosData const *infos, size_t count, bool texts)
{
  assert(target_infos_get_count(infos) == count);
  assert(target_infos_get_text_count(infos) == count * TargetInfoTextIndex_Count);

  for (size_t index = 0; index < count; ++index)
  {
    _Optional TargetInfo *const info = target_info_from_index(infos, index);
    assert(info);

    char const type[] = {(char)('A' + index), ':', '0', '\0'};
    char const details[] = {(char)('A' + index), ':', '1', '\0'};

    check_fields(&*info, position(index), texts ? type : "", texts ? details : "",
                 InfoField_AllMustMatch);
  }
}

static size_t write_coords(TargetInfosData *infos, bool padded,
  unsigned char (*buffer)[Capacity])
{
  memset(*buffer, 0, sizeof(*buffer));

  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));

  if (padded)
  {
    target_infos_write_pad(infos, &writer);
  }
  else
  {
    target_infos_write(infos, &writer);
  }

  assert(!writer_ferror(&writer));

  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  assert(writer_fputc(0, &writer) == 0);

  assert(writer_destroy(&writer) == length + 1);

  return (size_t)length;
}

static SFError read_coords(TargetInfosData *infos, bool padded,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  Reader reader;
  assert(length <= sizeof(*buffer));
  assert(reader_mem_init(&reader, *buffer, length));

  SFError const err = padded ?
    target_infos_read_pad(infos, &reader) : target_infos_read(infos, &reader);

  reader_destroy(&reader);

  return err;
}

static size_t write_texts(TargetInfosData *infos,
  long (*offsets)[OffsetCount], unsigned char (*buffer)[TextCapacity])
{
  /* Let Text's offset writer determine block extents; no encoded offsets
   * or header sizes are reproduced in the test. */
  unsigned char scratch[64];
  Writer writer;
  assert(writer_mem_init(&writer, scratch, sizeof(scratch)));

  int offset = 0;
  size_t next = 0;

  for (size_t index = 0; index < target_infos_get_count(infos); ++index)
  {
    _Optional TargetInfo *const info = target_info_from_index(infos, index);
    assert(info);

    for (TargetInfoTextIndex field = TargetInfoTextIndex_First;
         field < TargetInfoTextIndex_Count; ++field)
    {
      Text text;
      text_init(&text);
      assert(!SFError_fail(text_set_string(&text, target_info_get_text(&*info, field))));

      assert(next < ARRAY_SIZE(*offsets));
      (*offsets)[next++] = offset;
      text_write_offset(&text, &writer, &offset);

      text_destroy(&text);
    }
  }

  assert(!writer_ferror(&writer));

  assert(writer_destroy(&writer) >= 0);

  memset(*buffer, 0, sizeof(*buffer));
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));

  target_infos_write_texts(infos, &writer);

  assert(!writer_ferror(&writer));

  long const length = writer_ftell(&writer);
  assert(length == offset);
  assert((size_t)length < sizeof(*buffer));
  assert(writer_fputc(0, &writer) == 0);

  assert(writer_destroy(&writer) == length + 1);

  return (size_t)length;
}

static SFError read_texts(TargetInfosData *infos,
  long C23_CONST (*offsets)[OffsetCount], size_t count,
  unsigned char C23_CONST (*buffer)[TextCapacity], size_t length)
{
  Reader reader;
  assert(length <= sizeof(*buffer));
  assert(reader_mem_init(&reader, *buffer, length));

  SFError const err = target_infos_read_texts(infos, *offsets, count, &reader);

  reader_destroy(&reader);

  return err;
}

static void test1(void)
{
  /* Initially empty targets */

  TargetInfosData infos;
  target_infos_init(&infos);

  assert(target_infos_get_count(&infos) == 0);
  assert(target_infos_get_text_count(&infos) == 0);

  target_infos_destroy(&infos);
}

static void test2(void)
{
  /* Initial target text */

  TargetInfosData infos;
  target_infos_init(&infos);

  size_t index;
  assert(!SFError_fail(target_infos_add(&infos, (MapPoint){17, 29}, &index)));

  _Optional TargetInfo *const info = target_info_from_index(&infos, index);
  assert(info);

  check_fields(&*info, (MapPoint){17, 29}, "", "", InfoField_AllMustMatch);

  target_infos_destroy(&infos);
}

static void test3(void)
{
  /* Added target values */

  for (size_t count = 0; count <= TargetInfoMax; ++count)
  {
    TargetInfosData infos;
    populate(&infos, count);

    check_pattern(&infos, count, true);

    target_infos_destroy(&infos);
  }
}

static void test4(void)
{
  /* Target coordinate wrapping */

  TargetInfosData infos;
  target_infos_init(&infos);

  for (MapCoord shift = -2; shift <= 2; ++shift)
  {
    size_t index;
    assert(!SFError_fail(target_infos_add(&infos,
      (MapPoint){17 + shift * Map_Size, 29 - shift * Map_Size}, &index)));

    _Optional TargetInfo *const info = target_info_from_index(&infos, index);
    assert(info);

    check_fields(&*info, (MapPoint){17, 29}, "", "", InfoField_AllMustMatch);
  }

  assert(target_infos_get_count(&infos) == 5);

  target_infos_destroy(&infos);
}

static void test5(void)
{
  /* Target coordinate order */

  TargetInfosData infos;
  target_infos_init(&infos);

  for (size_t remaining = TargetInfoMax; remaining > 0; --remaining)
  {
    size_t index;
    assert(!SFError_fail(target_infos_add(&infos, position(remaining - 1), &index)));
    assert(index == 0);
  }

  for (size_t index = 0; index < TargetInfoMax; ++index)
  {
    _Optional TargetInfo *const info = target_info_from_index(&infos, index);
    assert(info);

    MapPoint const actual = target_info_get_pos(&*info);
    assert(actual.x == position(index).x);
    assert(actual.y == position(index).y);
  }

  target_infos_destroy(&infos);
}

static void test6(void)
{
  /* Target text replacement */

  for (TargetInfoTextIndex field = TargetInfoTextIndex_First;
       field < TargetInfoTextIndex_Count; ++field)
  {
    TargetInfosData infos;
    populate(&infos, 1);

    _Optional TargetInfo *const info = target_info_from_index(&infos, 0);
    assert(info);

    char string[] = "REPLACEMENT";
    assert(!SFError_fail(target_info_set_text(&*info, field, string)));
    string[0] = 'X';

    assert(strcmp(target_info_get_text(&*info, field), "REPLACEMENT") == 0);

    check_fields(&*info, position(0), "A:0", "A:1",
                 field == TargetInfoTextIndex_Type ? InfoField_Type : InfoField_Details);

    target_infos_destroy(&infos);
  }
}

static void test7(void)
{
  /* Target text clearing */

  TargetInfosData infos;
  populate(&infos, 1);

  _Optional TargetInfo *const info = target_info_from_index(&infos, 0);
  assert(info);

  assert(!SFError_fail(target_info_set_text(&*info, TargetInfoTextIndex_Type, "")));

  check_fields(&*info, position(0), "", "A:1", InfoField_AllMustMatch);

  target_infos_destroy(&infos);
}

static void test8(void)
{
  /* Target movement */

  TargetInfosData infos;
  populate(&infos, 3);

  _Optional TargetInfo *const moved = target_info_from_index(&infos, 1);
  assert(moved);

  int const id = target_info_get_id(&*moved);

  size_t const index = target_info_set_pos(&*moved, (MapPoint){-1, Map_Size + 7});
  assert(target_info_from_index(&infos, index) == moved);

  MapPoint const pos = target_info_get_pos(&*moved);
  assert(pos.x == Map_Size - 1);
  assert(pos.y == 7);
  assert(target_info_get_id(&*moved) == id);

  check_fields(&*moved, position(1), "B:0", "B:1", InfoField_Position);
  assert(target_infos_get_count(&infos) == 3);

  for (size_t other = 0; other < 2; ++other)
  {
    size_t const original = other * 2;
    _Optional TargetInfo *const info = target_info_from_index(&infos, other);
    assert(info);

    char const type[] = {(char)('A' + original), ':', '0', '\0'};
    char const details[] = {(char)('A' + original), ':', '1', '\0'};

    check_fields(&*info, position(original), type, details, InfoField_AllMustMatch);
  }

  target_infos_destroy(&infos);
}

static void test9(void)
{
  /* Target deletion */

  for (size_t removed = 0; removed < 3; ++removed)
  {
    TargetInfosData infos;
    populate(&infos, 3);

    _Optional TargetInfo *const info = target_info_from_index(&infos, removed);
    assert(info);

    assert(target_info_delete(&*info) == removed);
    assert(target_infos_get_count(&infos) == 2);

    for (size_t index = 0; index < 2; ++index)
    {
      size_t const original = index < removed ? index : index + 1;
      _Optional TargetInfo *const survivor = target_info_from_index(&infos, index);
      assert(survivor);

      char const type[] = {(char)('A' + original), ':', '0', '\0'};
      char const details[] = {(char)('A' + original), ':', '1', '\0'};

      check_fields(&*survivor, position(original), type, details, InfoField_AllMustMatch);
      assert(target_info_get_id(&*survivor) == (int)original);
    }

    target_infos_destroy(&infos);
  }
}

static void test10(void)
{
  /* Coincident target deletion */

  for (size_t removed = 0; removed < 3; ++removed)
  {
    TargetInfosData infos;
    target_infos_init(&infos);

    for (size_t index = 0; index < 3; ++index)
    {
      assert(!SFError_fail(target_infos_add(&infos, (MapPoint){5, 7}, NULL)));
    }

    _Optional TargetInfo *const info = target_info_from_index(&infos, removed);
    assert(info);

    int const id = target_info_get_id(&*info);
    assert(target_info_delete(&*info) == removed);
    assert(target_infos_get_count(&infos) == 2);

    for (size_t index = 0; index < 2; ++index)
    {
      _Optional TargetInfo *const survivor = target_info_from_index(&infos, index);
      assert(survivor);

      assert(target_info_get_id(&*survivor) != id);
    }

    target_infos_destroy(&infos);
  }
}

static void test11(void)
{
  /* Target capacity */

  TargetInfosData infos;
  populate(&infos, TargetInfoMax);

  assert(target_infos_add(&infos, (MapPoint){11, 13}, NULL).type == SFErrorType_NumInfos);

  check_pattern(&infos, TargetInfoMax, true);

  _Optional TargetInfo *const info = target_info_from_index(&infos, 0);
  assert(info);

  target_info_delete(&*info);
  assert(!SFError_fail(target_infos_add(&infos, (MapPoint){11, 13}, NULL)));
  assert(target_infos_get_count(&infos) == TargetInfoMax);

  target_infos_destroy(&infos);
}

static void test12(void)
{
  /* Target identifier stability */

  TargetInfosData infos;
  populate(&infos, 2);

  _Optional TargetInfo *const first = target_info_from_index(&infos, 0);
  _Optional TargetInfo *const second = target_info_from_index(&infos, 1);
  assert(first);
  assert(second);

  int const id = target_info_get_id(&*second);
  assert(target_info_get_id(&*first) != id);
  target_info_delete(&*first);

  size_t index;
  assert(!SFError_fail(target_infos_add(&infos, position(0), &index)));

  _Optional TargetInfo *const added = target_info_from_index(&infos, index);
  assert(added);

  assert(target_info_get_id(&*added) != id);
  assert(target_info_get_id(&*second) == id);

  target_infos_destroy(&infos);
}

static void test13(void)
{
  /* Target text width limit */

  for (TargetInfoTextIndex field = TargetInfoTextIndex_First;
       field < TargetInfoTextIndex_Count; ++field)
  {
    TargetInfosData infos;
    populate(&infos, 1);

    _Optional TargetInfo *const info = target_info_from_index(&infos, 0);
    assert(info);

    enum { StringCapacity = 64 };
    bool rejected = false;

    for (size_t length = 0; length + 1 < StringCapacity; ++length)
    {
      char string[StringCapacity];
      memset(string, 'A', length);
      string[length] = '\0';

      SFError const err = target_info_set_text(&*info, field, string);

      if (SFError_fail(err))
      {
        assert(err.type == SFErrorType_TooManyBriefingColumns);
        assert(length > 0);
        string[length - 1] = '\0';
        assert(strcmp(target_info_get_text(&*info, field), string) == 0);
        rejected = true;
      }
      else
      {
        assert(strcmp(target_info_get_text(&*info, field), string) == 0);
      }

      if (rejected)
      {
        break;
      }
    }

    assert(rejected);

    check_fields(&*info, position(0), "A:0", "A:1",
                 field == TargetInfoTextIndex_Type ? InfoField_Type : InfoField_Details);

    target_infos_destroy(&infos);
  }
}

static void test14(void)
{
  /* Target text line limit */

  for (TargetInfoTextIndex field = TargetInfoTextIndex_First;
       field < TargetInfoTextIndex_Count; ++field)
  {
    TargetInfosData infos;
    populate(&infos, 1);

    _Optional TargetInfo *const info = target_info_from_index(&infos, 0);
    assert(info);

    enum { StringCapacity = 64 };
    bool rejected = false;

    for (size_t length = 0; length + 1 < StringCapacity; ++length)
    {
      char string[StringCapacity];
      memset(string, '\n', length);
      string[length] = '\0';

      SFError const err = target_info_set_text(&*info, field, string);

      if (SFError_fail(err))
      {
        assert(err.type == SFErrorType_TooManyBriefingLines);
        assert(length > 0);
        string[length - 1] = '\0';
        assert(strcmp(target_info_get_text(&*info, field), string) == 0);
        rejected = true;
      }
      else
      {
        assert(strcmp(target_info_get_text(&*info, field), string) == 0);
      }

      if (rejected)
      {
        break;
      }
    }

    assert(rejected);

    check_fields(&*info, position(0), "A:0", "A:1",
                 field == TargetInfoTextIndex_Type ? InfoField_Type : InfoField_Details);

    target_infos_destroy(&infos);
  }
}

static void test15(void)
{
  /* Spatial target iteration */

  TargetInfosData infos;
  populate(&infos, TargetInfoMax);

  MapArea const area = {.min = {16, 0}, .max = {16, 16}};
  TargetInfosIter iter;
  size_t count = 0;

  for (size_t index = TargetInfosIter_get_first(&iter, &infos, &area);
       !TargetInfosIter_done(&iter); index = TargetInfosIter_get_next(&iter))
  {
    assert(index == 1 + count * 3);

    _Optional TargetInfo *const info = target_info_from_index(&infos, index);
    assert(info);

    MapPoint const pos = target_info_get_pos(&*info);
    assert(pos.x == 16);
    assert(pos.y == (MapCoord)count * 16);
    ++count;
  }

  assert(count == 2);

  target_infos_destroy(&infos);
}

static void test16(void)
{
  /* Empty spatial iteration */

  TargetInfosData infos;
  populate(&infos, TargetInfoMax);

  MapArea const area = {.min = {100, 100}, .max = {110, 110}};
  TargetInfosIter iter;
  TargetInfosIter_get_first(&iter, &infos, &area);
  assert(TargetInfosIter_done(&iter));

  target_infos_destroy(&infos);
}

static void test17(void)
{
  /* Deletion during spatial iteration */

  TargetInfosData infos;
  populate(&infos, TargetInfoMax);

  MapArea const area = {.min = {16, 0}, .max = {16, 16}};
  TargetInfosIter iter;
  size_t count = 0;

  for (TargetInfosIter_get_first(&iter, &infos, &area);
       !TargetInfosIter_done(&iter); TargetInfosIter_get_next(&iter))
  {
    TargetInfosIter_del_current(&iter);
    ++count;
  }

  assert(count == 2);
  assert(target_infos_get_count(&infos) == TargetInfoMax - count);

  for (size_t index = 0; index < target_infos_get_count(&infos); ++index)
  {
    _Optional TargetInfo *const info = target_info_from_index(&infos, index);
    assert(info);

    MapPoint const pos = target_info_get_pos(&*info);
    assert(pos.x != 16);
  }

  target_infos_destroy(&infos);
}

static void test18(void)
{
  /* Target coordinate round trip */

  for (int padded = 0; padded < 2; ++padded)
  {
    for (size_t count = 0; count <= TargetInfoMax; ++count)
    {
      TargetInfosData source;
      populate(&source, count);

      unsigned char buffer[Capacity];
      size_t const length = write_coords(&source, padded != 0, &buffer);

      TargetInfosData loaded;
      target_infos_init(&loaded);

      assert(!SFError_fail(read_coords(&loaded, padded != 0, &buffer, length)));

      check_pattern(&loaded, count, false);

      target_infos_destroy(&loaded);
      target_infos_destroy(&source);
    }
  }
}

static void test19(void)
{
  /* Target text round trip */

  for (size_t count = 0; count <= TargetInfoMax; ++count)
  {
    TargetInfosData source;
    populate(&source, count);

    unsigned char buffer[Capacity];
    size_t const coord_length = write_coords(&source, false, &buffer);
    static unsigned char text_data[TextCapacity];
    long offsets[OffsetCount];
    size_t const text_length = write_texts(&source, &offsets, &text_data);

    TargetInfosData loaded;
    target_infos_init(&loaded);

    assert(!SFError_fail(read_coords(&loaded, false, &buffer, coord_length)));
    assert(!SFError_fail(read_texts(&loaded, &offsets, count, &text_data, text_length)));

    check_pattern(&loaded, count, true);

    target_infos_destroy(&loaded);
    target_infos_destroy(&source);
  }
}

static void test20(void)
{
  /* Target coordinate truncation */

  TargetInfosData source;
  populate(&source, TargetInfoMax);

  unsigned char buffer[Capacity];
  size_t const size = write_coords(&source, false, &buffer);

  for (size_t length = 0; length < size; ++length)
  {
    TargetInfosData loaded;
    target_infos_init(&loaded);

    SFError const err = read_coords(&loaded, false, &buffer, length);

    if (length == 0)
    {
      assert(SFError_fail(err));
    }

    if (!SFError_fail(err))
    {

      check_pattern(&loaded, TargetInfoMax, false);
    }

    target_infos_destroy(&loaded);
  }

  target_infos_destroy(&source);
}

static void test21(void)
{
  /* Corrupt target coordinates */

  TargetInfosData source;
  populate(&source, 2);

  unsigned char buffer[Capacity];
  size_t const length = write_coords(&source, false, &buffer);
  bool bad_count = false, read_fail = false;

  for (size_t offset = 0; offset < length; ++offset)
  {
    unsigned char const original = buffer[offset];

    for (unsigned byte = 0; byte <= UCHAR_MAX; ++byte)
    {
      buffer[offset] = (unsigned char)byte;

      TargetInfosData loaded;
      target_infos_init(&loaded);

      SFError const err = read_coords(&loaded, false, &buffer, length);

      switch (err.type)
      {
        case SFErrorType_OK: break;
        case SFErrorType_BadNumTargetInfo:
          bad_count = true;
          break;

        case SFErrorType_ReadFail:
          read_fail = true;
          break;

        default:
          assert(false);
          break;
      }

      target_infos_destroy(&loaded);
    }

    buffer[offset] = original;
  }

  assert(bad_count);
  assert(read_fail);

  target_infos_destroy(&source);
}

static void test22(void)
{
  /* Target text truncation */

  TargetInfosData source;
  populate(&source, 1);

  static unsigned char text_data[TextCapacity];
  long offsets[OffsetCount];
  size_t const size = write_texts(&source, &offsets, &text_data);

  for (size_t length = 0; length < size; ++length)
  {
    TargetInfosData loaded;
    target_infos_init(&loaded);

    assert(!SFError_fail(target_infos_add(&loaded, position(0), NULL)));

    SFError const err = read_texts(&loaded, &offsets, 1, &text_data, length);

    if (length == 0)
    {
      assert(SFError_fail(err));
    }

    if (!SFError_fail(err))
    {

      check_pattern(&loaded, 1, true);
    }

    target_infos_destroy(&loaded);
  }

  target_infos_destroy(&source);
}

static void test23(void)
{
  /* Invalid target text offset */

  TargetInfosData source;
  populate(&source, 1);

  static unsigned char text_data[TextCapacity];
  long offsets[OffsetCount];
  size_t const length = write_texts(&source, &offsets, &text_data);

  for (size_t field = 0; field < TargetInfoTextIndex_Count; ++field)
  {
    long const original = offsets[field];
    offsets[field] = -1;

    TargetInfosData loaded;
    populate(&loaded, 1);

    assert(read_texts(&loaded, &offsets, 1, &text_data, length).type == SFErrorType_BadSeek);

    target_infos_destroy(&loaded);

    offsets[field] = original;
  }

  target_infos_destroy(&source);
}

static void test24(void)
{
  /* Partial target text loading */

  TargetInfosData source;
  populate(&source, 2);

  static unsigned char text_data[TextCapacity];
  long offsets[OffsetCount];
  size_t const length = write_texts(&source, &offsets, &text_data);

  TargetInfosData loaded;
  target_infos_init(&loaded);

  for (size_t index = 0; index < 2; ++index)
  {
    assert(!SFError_fail(target_infos_add(&loaded, position(index), NULL)));
  }

  assert(!SFError_fail(read_texts(&loaded, &offsets, 1, &text_data, length)));

  _Optional TargetInfo *const first = target_info_from_index(&loaded, 0);
  _Optional TargetInfo *const second = target_info_from_index(&loaded, 1);
  assert(first);
  assert(second);

  check_fields(&*first, position(0), "A:0", "A:1", InfoField_AllMustMatch);
  check_fields(&*second, position(1), "", "", InfoField_AllMustMatch);

  target_infos_destroy(&loaded);
  target_infos_destroy(&source);
}

static void test25(void)
{
  /* Target offset extent */

  TargetInfosData infos;
  populate(&infos, TargetInfoMax);

  static unsigned char text_data[TextCapacity];
  long offsets[OffsetCount];
  size_t const length = write_texts(&infos, &offsets, &text_data);
  unsigned char buffer[Capacity];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, sizeof(buffer)));

  int const start = 37;
  assert(target_infos_write_text_offsets(&infos, &writer, start) == start + (int)length);
  assert(!writer_ferror(&writer));

  assert(writer_destroy(&writer) > 0);
  target_infos_destroy(&infos);
}

static void test26(void)
{
  /* Target coordinate output failure */

  TargetInfosData infos;
  populate(&infos, 2);

  unsigned char buffer[Capacity];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));

  target_infos_write(&infos, &writer);

  assert(writer_ferror(&writer));

  writer_destroy(&writer);
  target_infos_destroy(&infos);
}

#ifdef FORTIFY
static void test27(void)
{
  /* Target allocation failure */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    TargetInfosData infos;
    target_infos_init(&infos);

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = target_infos_add(&infos, position(0), NULL);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
      assert(target_infos_get_count(&infos) == 0);
      assert(!SFError_fail(target_infos_add(&infos, position(0), NULL)));
    }
    else
    {
      assert(limit > 0);
      assert(target_infos_get_count(&infos) == 1);
      succeeded = true;
    }

    target_infos_destroy(&infos);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);
}

#endif

#ifdef FORTIFY
static void test29(void)
{
  /* Target read allocation failure */

  TargetInfosData source;
  populate(&source, 2);

  unsigned char buffer[Capacity];
  size_t const length = write_coords(&source, false, &buffer);
  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    TargetInfosData loaded;
    target_infos_init(&loaded);

    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = target_infos_read(&loaded, &reader);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
    }
    else
    {
      assert(limit > 0);

      check_pattern(&loaded, 2, false);
      succeeded = true;
    }

    target_infos_destroy(&loaded);
    reader_destroy(&reader);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);

  target_infos_destroy(&source);
}

#endif

static void test30(void)
{
  /* Padded target output failure */

  TargetInfosData infos;
  populate(&infos, 2);

  unsigned char buffer[Capacity];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));

  target_infos_write_pad(&infos, &writer);

  assert(writer_ferror(&writer));

  writer_destroy(&writer);
  target_infos_destroy(&infos);
}

static void test31(void)
{
  /* Target text output failure */

  TargetInfosData infos;
  populate(&infos, 2);

  unsigned char buffer[Capacity];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));

  target_infos_write_texts(&infos, &writer);

  assert(writer_ferror(&writer));

  writer_destroy(&writer);
  target_infos_destroy(&infos);
}

static void test32(void)
{
  /* Target offset output failure */

  TargetInfosData infos;
  populate(&infos, 2);

  unsigned char buffer[Capacity];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));

  target_infos_write_text_offsets(&infos, &writer, 0);

  assert(writer_ferror(&writer));

  writer_destroy(&writer);
  target_infos_destroy(&infos);
}

static void test33(void)
{
  /* Wrapped spatial iteration */

  static MapPoint const points[] = {
    {0, 0}, {Map_Size - 1, Map_Size - 1}, {1, 0}, {0, 1},
    {Map_Size - 16, Map_Size - 16}, {Map_Size - 17, Map_Size - 17},
  };

  TargetInfosData infos;
  target_infos_init(&infos);

  for (size_t index = 0; index < ARRAY_SIZE(points); ++index)
  {
    assert(!SFError_fail(target_infos_add(&infos, points[index], NULL)));
  }

  MapArea const area = {.min = {-16, -16}, .max = {0, 0}};
  TargetInfosIter iter;
  size_t count = 0;

  for (size_t index = TargetInfosIter_get_first(&iter, &infos, &area);
       !TargetInfosIter_done(&iter); index = TargetInfosIter_get_next(&iter))
  {
    _Optional TargetInfo *const info = target_info_from_index(&infos, index);
    assert(info);

    MapPoint const pos = target_info_get_pos(&*info);
    assert(pos.x == pos.y);

    if (pos.x != 0)
    {
      assert(pos.x >= Map_Size - 16);
    }

    ++count;
  }

  assert(count == 3);

  target_infos_destroy(&infos);
}

static void test34(void)
{
  /* Target coordinate stream position */

  for (int padded = 0; padded < 2; ++padded)
  {
    TargetInfosData source;
    populate(&source, 2);

    unsigned char buffer[Capacity];
    size_t const length = write_coords(&source, padded != 0, &buffer);

    TargetInfosData loaded;
    target_infos_init(&loaded);

    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));

    SFError const err = padded ?
      target_infos_read_pad(&loaded, &reader) : target_infos_read(&loaded, &reader);
    assert(!SFError_fail(err));
    assert(reader_ftell(&reader) == (long)length);

    reader_destroy(&reader);
    target_infos_destroy(&loaded);
    target_infos_destroy(&source);
  }
}

#ifdef FORTIFY
static void test35(void)
{
  /* Target text read allocation failure */

  TargetInfosData source;
  populate(&source, 1);

  static unsigned char text_data[TextCapacity];
  long offsets[OffsetCount];
  size_t const length = write_texts(&source, &offsets, &text_data);
  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    TargetInfosData loaded;
    target_infos_init(&loaded);

    assert(!SFError_fail(target_infos_add(&loaded, position(0), NULL)));

    Reader reader;
    assert(reader_mem_init(&reader, text_data, length));

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = target_infos_read_texts(&loaded, offsets, 1, &reader);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
    }
    else
    {
      assert(limit > 0);

      check_pattern(&loaded, 1, true);
      succeeded = true;
    }

    target_infos_destroy(&loaded);
    reader_destroy(&reader);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);

  target_infos_destroy(&source);
}

#endif

void Infos_tests(void)
{
  /* Infos */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initially empty targets", test1 },
    { "Initial target text", test2 },
    { "Added target values", test3 },
    { "Target coordinate wrapping", test4 },
    { "Target coordinate order", test5 },
    { "Target text replacement", test6 },
    { "Target text clearing", test7 },
    { "Target movement", test8 },
    { "Target deletion", test9 },
    { "Coincident target deletion", test10 },
    { "Target capacity", test11 },
    { "Target identifier stability", test12 },
    { "Target text width limit", test13 },
    { "Target text line limit", test14 },
    { "Spatial target iteration", test15 },
    { "Empty spatial iteration", test16 },
    { "Deletion during spatial iteration", test17 },
    { "Target coordinate round trip", test18 },
    { "Target text round trip", test19 },
    { "Target coordinate truncation", test20 },
    { "Corrupt target coordinates", test21 },
    { "Target text truncation", test22 },
    { "Invalid target text offset", test23 },
    { "Partial target text loading", test24 },
    { "Target offset extent", test25 },
    { "Target coordinate output failure", test26 },
    { "Padded target output failure", test30 },
    { "Target text output failure", test31 },
    { "Target offset output failure", test32 },
    { "Wrapped spatial iteration", test33 },
    { "Target coordinate stream position", test34 },
#ifdef FORTIFY
    { "Target allocation failure", test27 },
#endif
#ifdef FORTIFY
    { "Target read allocation failure", test29 },
    { "Target text read allocation failure", test35 },
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
