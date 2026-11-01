/* SFEditor test: mission triggers.
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
#include "../Triggers.h"
#include "../TriggersData.h"
#include "../Obj.h"
#include "../Ships.h"

/* Enough for a full padded trigger list, without encoding its size here. */
enum { Capacity = 512 };

static MapArea whole_map(void)
{
  return (MapArea){.min = {0, 0}, .max = {Obj_Size - 1, Obj_Size - 1}};
}

static MapPoint position(size_t index)
{
  return (MapPoint){1 + (MapCoord)(index % 8) * 3,
                    2 + (MapCoord)(index / 8) * 5};
}

static TriggerFullParam parameter(size_t index)
{
  return (TriggerFullParam){
    .param = {.action = TriggerAction_MissionTarget, .value = (int)(index * 37 % 256)},
    .next_coords = {0, 0},
  };
}

static void check_point(MapPoint actual, MapPoint expected)
{
  assert(actual.x == expected.x);
  assert(actual.y == expected.y);
}

static void check_fields(TriggerFullParam const *actual, TriggerFullParam expected)
{
  assert(actual->param.action == expected.param.action);
  assert(actual->param.value == expected.param.value);
  check_point(actual->next_coords, expected.next_coords);
}

static void populate(TriggersData *triggers, size_t count)
{
  assert(!SFError_fail(triggers_init(triggers)));

  for (size_t i = 0; i < count; ++i)
  {
    assert(!SFError_fail(triggers_add(triggers, position(i), parameter(i))));
  }
}

static void check_one(TriggersData *triggers, MapPoint pos, TriggerFullParam expected)
{
  MapArea const area = {.min = pos, .max = pos};
  TriggersIter iter;
  TriggerFullParam actual;
  MapPoint const first = TriggersIter_get_first(&iter, triggers, &area, &actual);
  assert(!TriggersIter_done(&iter));
  check_point(first, pos);
  check_fields(&actual, expected);

  TriggersIter_get_next(&iter, &actual);
  assert(TriggersIter_done(&iter));
}

static void check_pattern(TriggersData *triggers, size_t count)
{
  assert(triggers_get_count(triggers) == count);

  MapArea const area = whole_map();
  TriggersIter iter;
  TriggerFullParam actual;
  size_t index = 0;

  for (MapPoint pos = TriggersIter_get_first(&iter, triggers, &area, &actual);
       !TriggersIter_done(&iter); pos = TriggersIter_get_next(&iter, &actual))
  {
    assert(index < count);
    check_point(pos, position(index));
    check_fields(&actual, parameter(index));
    ++index;
  }

  assert(index == count);
}

static TriggerFullParam chain_to(MapPoint next)
{
  return (TriggerFullParam){
    .param = {.action = TriggerAction_ChainReaction, .value = 17},
    .next_coords = next,
  };
}

static void check_chain(TriggersData *triggers, MapPoint from, MapPoint to)
{
  MapArea const area = {.min = to, .max = to};
  TriggersChainIter iter;
  TriggerFullParam actual;
  MapPoint const pos = TriggersChainIter_get_first(&iter, triggers, &area, &actual);
  assert(!TriggersChainIter_done(&iter));
  check_point(pos, from);
  check_fields(&actual, chain_to(to));

  TriggersChainIter_get_next(&iter, &actual);
  assert(TriggersChainIter_done(&iter));
}

static size_t write_data(TriggersData *triggers, bool padded,
  unsigned char (*buffer)[Capacity])
{
  memset(*buffer, 0, sizeof(*buffer));

  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));

  if (padded)
  {
    triggers_write_pad(triggers, &writer);
  }
  else
  {
    triggers_write(triggers, &writer);
  }

  assert(!writer_ferror(&writer));

  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);

  return (size_t)length;
}

static SFError read_data(TriggersData *triggers, bool padded,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));

  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));

  SFError const err = padded ? triggers_read_pad(triggers, &reader) :
                              triggers_read(triggers, &reader);

  reader_destroy(&reader);

  return err;
}

static void test1(void)
{
  /* Initial trigger count */

  TriggersData triggers;
  assert(!SFError_fail(triggers_init(&triggers)));

  assert(triggers_get_count(&triggers) == 0);
  assert(triggers_get_max_losses(&triggers) == 0);
  check_pattern(&triggers, 0);

  triggers_destroy(&triggers);
}

static void test2(void)
{
  /* Initial trigger locations */

  TriggersData triggers;
  populate(&triggers, 0);

  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      assert(!triggers_check_locn(&triggers, (MapPoint){x, y}));
    }
  }

  triggers_destroy(&triggers);
}

static void test3(void)
{
  /* Added trigger values */

  TriggersData triggers;
  populate(&triggers, TriggersMax);

  check_pattern(&triggers, TriggersMax);

  triggers_destroy(&triggers);
}

static void test4(void)
{
  /* Untouched trigger locations */

  TriggersData triggers;
  populate(&triggers, TriggersMax);

  for (MapCoord y = 0; y < Obj_Size; ++y)
  {
    for (MapCoord x = 0; x < Obj_Size; ++x)
    {
      bool const column = ((x % 3) == 1) && ((x / 3) < 8);
      bool const row = ((y % 5) == 2) && ((y / 5) < 8);
      bool const expected = column && row;
      assert(triggers_check_locn(&triggers, (MapPoint){x, y}) == expected);
    }
  }

  triggers_destroy(&triggers);
}

static void test5(void)
{
  /* Coincident triggers */

  TriggersData triggers;
  populate(&triggers, 0);

  for (int value = 0; value < 3; ++value)
  {
    assert(!SFError_fail(triggers_add(&triggers, position(0), (TriggerFullParam){
      .param = {.action = TriggerAction_CashBonus, .value = value},
      .next_coords = {0, 0},
    })));
  }

  assert(triggers_count_locn(&triggers, position(0)) == 3);

  MapArea const area = whole_map();
  TriggersIter iter;
  TriggerFullParam actual;
  bool seen[3] = {false};
  size_t count = 0;

  for (MapPoint pos = TriggersIter_get_first(&iter, &triggers, &area, &actual);
       !TriggersIter_done(&iter); pos = TriggersIter_get_next(&iter, &actual))
  {
    check_point(pos, position(0));
    assert(actual.param.action == TriggerAction_CashBonus);
    assert(actual.param.value >= 0);
    assert(actual.param.value < 3);
    assert(!seen[actual.param.value]);
    seen[actual.param.value] = true;
    ++count;
  }

  assert(count == 3);

  triggers_destroy(&triggers);
}

static void test6(void)
{
  /* Trigger coordinate wrapping */

  TriggersData triggers;
  populate(&triggers, 0);
  assert(!SFError_fail(triggers_add(&triggers,
    (MapPoint){Obj_Size + 3, -1}, parameter(0))));

  check_one(&triggers, (MapPoint){3, Obj_Size - 1}, parameter(0));
  assert(triggers_check_locn(&triggers, (MapPoint){3 - Obj_Size, Obj_Size * 2 - 1}));
  assert(triggers_count_locn(&triggers, (MapPoint){3, -1}) == 1);

  triggers_destroy(&triggers);
}

static void test7(void)
{
  /* Spatial trigger iteration */

  TriggersData triggers;
  populate(&triggers, 16);

  MapArea const area = {.min = {4, 2}, .max = {4, 7}};
  assert(triggers_count_bbox(&triggers, &area) == 2);

  TriggersIter iter;
  TriggerFullParam actual;
  size_t count = 0;

  for (MapPoint pos = TriggersIter_get_first(&iter, &triggers, &area, &actual);
       !TriggersIter_done(&iter); pos = TriggersIter_get_next(&iter, &actual))
  {
    assert(count < 2);
    size_t const index = 1 + count * 8;
    check_point(pos, position(index));
    check_fields(&actual, parameter(index));
    ++count;
  }

  assert(count == 2);

  triggers_destroy(&triggers);
}

static void test8(void)
{
  /* Iteration without parameters */

  TriggersData triggers;
  populate(&triggers, 16);

  MapArea const area = whole_map();
  TriggersIter iter;
  size_t count = 0;

  for (MapPoint pos = TriggersIter_get_first(&iter, &triggers, &area, NULL);
       !TriggersIter_done(&iter); pos = TriggersIter_get_next(&iter, NULL))
  {
    assert(count < 16);
    check_point(pos, position(count));
    ++count;
  }

  assert(count == 16);

  triggers_destroy(&triggers);
}

static void test9(void)
{
  /* Wrapped trigger area */

  TriggersData triggers;
  populate(&triggers, 0);

  static MapPoint const points[] = {
    {0, 0}, {Obj_Size - 1, 0}, {0, Obj_Size - 1},
    {Obj_Size - 1, Obj_Size - 1}, {1, 0}, {0, 1},
  };

  for (size_t i = 0; i < ARRAY_SIZE(points); ++i)
  {
    assert(!SFError_fail(triggers_add(&triggers, points[i], parameter(i))));
  }

  MapArea const area = {.min = {-1, -1}, .max = {0, 0}};
  assert(triggers_count_bbox(&triggers, &area) == 4);

  TriggersIter iter;
  size_t count = 0;

  for (MapPoint pos = TriggersIter_get_first(&iter, &triggers, &area, NULL);
       !TriggersIter_done(&iter); pos = TriggersIter_get_next(&iter, NULL))
  {
    assert((pos.x + 1) % Obj_Size < 2);
    assert((pos.y + 1) % Obj_Size < 2);
    ++count;
  }

  assert(count == 4);

  triggers_destroy(&triggers);
}

static void test10(void)
{
  /* Trigger deletion during iteration */

  TriggersData triggers;
  populate(&triggers, 16);

  MapArea const area = whole_map();
  TriggersIter iter;
  size_t index = 0;

  for (TriggersIter_get_first(&iter, &triggers, &area, NULL);
       !TriggersIter_done(&iter); TriggersIter_get_next(&iter, NULL))
  {
    if (index % 2 == 0)
    {
      TriggersIter_del_current(&iter);
    }
    ++index;
  }

  triggers_cleanup(&triggers);
  assert(index == 16);
  assert(triggers_get_count(&triggers) == 8);

  for (size_t i = 0; i < 16; ++i)
  {
    assert(triggers_check_locn(&triggers, position(i)) == (i % 2 != 0));

    if (i % 2 != 0)
    {
      check_one(&triggers, position(i), parameter(i));
    }
  }

  triggers_cleanup(&triggers);
  assert(triggers_get_count(&triggers) == 8);

  triggers_destroy(&triggers);
}

static void test11(void)
{
  /* Trigger capacity */

  TriggersData triggers;
  populate(&triggers, TriggersMax);

  assert(triggers_add(&triggers, (MapPoint){100, 100}, parameter(0)).type ==
         SFErrorType_NumActions);
  check_pattern(&triggers, TriggersMax);

  triggers_destroy(&triggers);
}

static void test12(void)
{
  /* Maximum losses setter */

  TriggersData triggers;
  populate(&triggers, TriggersMax);

  for (size_t losses = 0; losses <= TriggersMax; ++losses)
  {
    triggers_set_max_losses(&triggers, losses);
    assert(triggers_get_max_losses(&triggers) == losses);
  }

  check_pattern(&triggers, TriggersMax);

  triggers_destroy(&triggers);
}

static void test13(void)
{
  /* Chain with implicit destination */

  TriggersData triggers;
  populate(&triggers, 0);
  assert(!SFError_fail(triggers_add(&triggers, position(0), chain_to(position(1)))));

  assert(triggers_get_count(&triggers) == 2);
  check_one(&triggers, position(0), chain_to(position(1)));
  check_chain(&triggers, position(0), position(1));

  triggers_destroy(&triggers);
}

static void test14(void)
{
  /* Chain with existing destination */

  TriggersData triggers;
  populate(&triggers, 1);
  assert(!SFError_fail(triggers_add(&triggers, position(1), chain_to(position(0)))));

  assert(triggers_get_count(&triggers) == 2);
  check_one(&triggers, position(0), parameter(0));
  check_one(&triggers, position(1), chain_to(position(0)));
  check_chain(&triggers, position(1), position(0));

  triggers_destroy(&triggers);
}

static void test15(void)
{
  /* Dummy destination replacement */

  TriggersData triggers;
  populate(&triggers, 0);
  assert(!SFError_fail(triggers_add(&triggers, position(0), chain_to(position(1)))));
  assert(!SFError_fail(triggers_add(&triggers, position(1), parameter(1))));

  assert(triggers_get_count(&triggers) == 2);
  check_one(&triggers, position(1), parameter(1));
  check_chain(&triggers, position(0), position(1));

  triggers_destroy(&triggers);
}

static void test16(void)
{
  /* Shared chain destination */

  TriggersData triggers;
  populate(&triggers, 0);

  for (size_t i = 0; i < 2; ++i)
  {
    assert(!SFError_fail(triggers_add(&triggers, position(i), chain_to(position(7)))));
  }

  MapArea const area = {.min = position(7), .max = position(7)};
  TriggersChainIter iter;
  TriggerFullParam actual;
  bool seen[2] = {false};
  size_t count = 0;

  for (MapPoint pos = TriggersChainIter_get_first(&iter, &triggers, &area, &actual);
       !TriggersChainIter_done(&iter); pos = TriggersChainIter_get_next(&iter, &actual))
  {
    assert(pos.x >= 1);
    size_t const index = (size_t)(pos.x - 1) / 3;
    assert(index < ARRAY_SIZE(seen));
    assert(!seen[index]);
    seen[index] = true;
    check_point(pos, position(index));
    check_fields(&actual, chain_to(position(7)));
    ++count;
  }

  assert(count == 2);

  triggers_destroy(&triggers);
}

static void test17(void)
{
  /* Chain deletion cleanup */

  TriggersData triggers;
  populate(&triggers, 0);
  assert(!SFError_fail(triggers_add(&triggers, position(0), chain_to(position(1)))));

  MapArea const area = whole_map();
  TriggersIter iter;
  TriggersIter_get_first(&iter, &triggers, &area, NULL);
  assert(!TriggersIter_done(&iter));
  TriggersIter_del_current(&iter);
  triggers_cleanup(&triggers);

  assert(triggers_get_count(&triggers) == 0);
  assert(!triggers_check_locn(&triggers, position(0)));
  assert(!triggers_check_locn(&triggers, position(1)));
  check_pattern(&triggers, 0);

  triggers_destroy(&triggers);
}

static void test18(void)
{
  /* Chain iterator deletion */

  TriggersData triggers;
  populate(&triggers, 0);
  assert(!SFError_fail(triggers_add(&triggers, position(0), chain_to(position(1)))));

  MapArea const area = {.min = position(1), .max = position(1)};
  TriggersChainIter iter;
  TriggerFullParam actual;
  TriggersChainIter_get_first(&iter, &triggers, &area, &actual);
  assert(!TriggersChainIter_done(&iter));
  TriggersChainIter_del_current(&iter);
  TriggersChainIter_get_next(&iter, &actual);
  assert(TriggersChainIter_done(&iter));

  assert(!SFError_fail(triggers_add(&triggers, position(1), parameter(1))));
  check_one(&triggers, position(1), parameter(1));
  assert(!triggers_check_locn(&triggers, position(0)));

  triggers_destroy(&triggers);
}

static void test19(void)
{
  /* Deletion preserves dependent chain */

  TriggersData triggers;
  populate(&triggers, 0);
  assert(!SFError_fail(triggers_add(&triggers, position(1), chain_to(position(2)))));
  assert(!SFError_fail(triggers_add(&triggers, position(0), chain_to(position(1)))));

  MapArea const area = {.min = position(2), .max = position(2)};
  TriggersChainIter iter;
  TriggerFullParam actual;
  TriggersChainIter_get_first(&iter, &triggers, &area, &actual);
  assert(!TriggersChainIter_done(&iter));
  TriggersChainIter_del_current(&iter);
  triggers_cleanup(&triggers);

  check_one(&triggers, position(0), chain_to(position(1)));
  check_chain(&triggers, position(0), position(1));

  triggers_destroy(&triggers);
}

static void test20(void)
{
  /* Self-referencing chain */

  TriggersData triggers;
  populate(&triggers, 0);
  assert(!SFError_fail(triggers_add(&triggers, position(0), chain_to(position(0)))));

  check_one(&triggers, position(0), chain_to(position(0)));
  check_chain(&triggers, position(0), position(0));

  MapArea const area = whole_map();
  TriggersIter iter;
  TriggersIter_get_first(&iter, &triggers, &area, NULL);
  TriggersIter_del_current(&iter);
  triggers_cleanup(&triggers);

  assert(triggers_get_count(&triggers) == 0);
  assert(!triggers_check_locn(&triggers, position(0)));

  triggers_destroy(&triggers);
}

static void test21(void)
{
  /* Chain capacity rollback */

  TriggersData triggers;
  populate(&triggers, TriggersMax - 1);

  assert(triggers_add(&triggers, (MapPoint){100, 100},
                     chain_to((MapPoint){101, 101})).type == SFErrorType_NumActions);
  triggers_cleanup(&triggers);

  check_pattern(&triggers, TriggersMax - 1);
  assert(!triggers_check_locn(&triggers, (MapPoint){100, 100}));
  assert(!triggers_check_locn(&triggers, (MapPoint){101, 101}));

  triggers_destroy(&triggers);
}

static void test22(void)
{
  /* Trigger round trip */

  for (int padded = 0; padded < 2; ++padded)
  {
    for (size_t count = 0; count <= TriggersMax; ++count)
    {
      TriggersData source;
      populate(&source, count);

      unsigned char buffer[Capacity];
      size_t const length = write_data(&source, padded != 0, &buffer);

      TriggersData loaded;
      populate(&loaded, 0);
      assert(!SFError_fail(read_data(&loaded, padded != 0, &buffer, length)));

      check_pattern(&loaded, count);

      triggers_destroy(&loaded);
      triggers_destroy(&source);
    }
  }
}

static void test23(void)
{
  /* Chain round trip */

  for (int padded = 0; padded < 2; ++padded)
  {
    TriggersData source;
    populate(&source, 0);
    assert(!SFError_fail(triggers_add(&source, position(1), chain_to(position(2)))));
    assert(!SFError_fail(triggers_add(&source, position(0), chain_to(position(1)))));

    unsigned char buffer[Capacity];
    size_t const length = write_data(&source, padded != 0, &buffer);

    TriggersData loaded;
    populate(&loaded, 0);
    assert(!SFError_fail(read_data(&loaded, padded != 0, &buffer, length)));

    check_one(&loaded, position(0), chain_to(position(1)));
    check_one(&loaded, position(1), chain_to(position(2)));
    check_chain(&loaded, position(0), position(1));
    check_chain(&loaded, position(1), position(2));

    triggers_destroy(&loaded);
    triggers_destroy(&source);
  }
}

static void test24(void)
{
  /* Trigger truncation */

  for (int padded = 0; padded < 2; ++padded)
  {
    TriggersData source;
    populate(&source, 3);

    unsigned char buffer[Capacity];
    size_t const size = write_data(&source, padded != 0, &buffer);

    for (size_t length = 0; length < size; ++length)
    {
      TriggersData loaded;
      populate(&loaded, 0);
      SFError const err = read_data(&loaded, padded != 0, &buffer, length);

      if (length == 0)
      {
        assert(SFError_fail(err));
      }

      if (!SFError_fail(err))
      {
        check_pattern(&loaded, 3);
      }

      triggers_destroy(&loaded);
    }

    triggers_destroy(&source);
  }
}

static void test25(void)
{
  /* Corrupt trigger data */

  TriggersData source;
  populate(&source, 2);
  assert(!SFError_fail(triggers_add(&source, position(2), (TriggerFullParam){
    .param = {.action = TriggerAction_CrippleShipType, .value = ShipType_Fighter1},
    .next_coords = {0, 0},
  })));

  unsigned char buffer[Capacity];
  size_t const length = write_data(&source, false, &buffer);
  static SFErrorType const expected[] = {
    SFErrorType_BadNumTriggers, SFErrorType_BadTriggerCoord,
    SFErrorType_BadTriggerAction, SFErrorType_BadTriggerShipType,
    SFErrorType_BadChainReaction, SFErrorType_ReadFail,
  };
  bool seen[ARRAY_SIZE(expected)] = {false};

  for (size_t offset = 0; offset < length; ++offset)
  {
    unsigned char const original = buffer[offset];

    for (unsigned byte = 0; byte <= UCHAR_MAX; ++byte)
    {
      buffer[offset] = (unsigned char)byte;

      TriggersData loaded;
      populate(&loaded, 0);
      SFError const err = read_data(&loaded, false, &buffer, length);

      if (SFError_fail(err))
      {
        size_t found = 0;
        while ((found < ARRAY_SIZE(expected)) && (expected[found] != err.type))
        {
          ++found;
        }
        assert(found < ARRAY_SIZE(expected));
        seen[found] = true;
      }

      triggers_destroy(&loaded);
    }

    buffer[offset] = original;
  }

  for (size_t i = 0; i < ARRAY_SIZE(seen); ++i)
  {
    assert(seen[i]);
  }

  triggers_destroy(&source);
}

static void test26(void)
{
  /* Trigger stream position */

  for (int padded = 0; padded < 2; ++padded)
  {
    TriggersData source;
    populate(&source, 3);

    unsigned char buffer[Capacity];
    size_t const length = write_data(&source, padded != 0, &buffer);

    TriggersData loaded;
    populate(&loaded, 0);
    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));

    SFError const err = padded ? triggers_read_pad(&loaded, &reader) :
                                triggers_read(&loaded, &reader);
    assert(!SFError_fail(err));
    assert(reader_ftell(&reader) == (long)length);

    reader_destroy(&reader);
    triggers_destroy(&loaded);
    triggers_destroy(&source);
  }
}

static void test27(void)
{
  /* Maximum losses round trip */

  TriggersData source;
  populate(&source, TriggersMax);

  for (size_t value = 0; value <= TriggersMax; ++value)
  {
    triggers_set_max_losses(&source, value);

    unsigned char buffer[64];
    Writer writer;
    assert(writer_mem_init(&writer, buffer, sizeof(buffer)));
    triggers_write_max_losses(&source, &writer);
    assert(!writer_ferror(&writer));
    long const length = writer_ftell(&writer);
    assert(length > 0);
    assert(writer_destroy(&writer) == length);

    TriggersData loaded;
    populate(&loaded, 0);
    Reader reader;
    assert(reader_mem_init(&reader, buffer, (size_t)length));
    assert(!SFError_fail(triggers_read_max_losses(&loaded, &reader)));
    assert(triggers_get_max_losses(&loaded) == value);

    reader_destroy(&reader);
    triggers_destroy(&loaded);
  }

  triggers_destroy(&source);
}

static void test28(void)
{
  /* Trigger output failure */

  for (int padded = 0; padded < 2; ++padded)
  {
    TriggersData source;
    populate(&source, 1);

    unsigned char buffer[Capacity];
    Writer writer;
    assert(writer_mem_init(&writer, buffer, 0));

    if (padded)
    {
      triggers_write_pad(&source, &writer);
    }
    else
    {
      triggers_write(&source, &writer);
    }

    assert(writer_ferror(&writer));

    writer_destroy(&writer);
    triggers_destroy(&source);
  }
}

#ifdef FORTIFY
static void test29(void)
{
  /* Trigger initial allocation failure */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 8; ++limit)
  {
    TriggersData triggers;
    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = triggers_init(&triggers);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
    }
    else
    {
      assert(limit > 0);
      check_pattern(&triggers, 0);
      triggers_destroy(&triggers);
      succeeded = true;
      break;
    }
  }

  assert(succeeded);
}

static void test30(void)
{
  /* Trigger add allocation failure */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    TriggersData triggers;
    populate(&triggers, 1);
    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = triggers_add(&triggers, position(1), parameter(1));
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
      check_pattern(&triggers, 1);
    }
    else
    {
      assert(limit > 0);
      check_pattern(&triggers, 2);
      succeeded = true;
    }

    triggers_destroy(&triggers);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);
}

static void test31(void)
{
  /* Chain allocation rollback */

  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    TriggersData triggers;
    populate(&triggers, 0);
    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = triggers_add(&triggers, position(0), chain_to(position(1)));
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
      triggers_cleanup(&triggers);
      check_pattern(&triggers, 0);
      assert(!triggers_check_locn(&triggers, position(0)));
      assert(!triggers_check_locn(&triggers, position(1)));
    }
    else
    {
      assert(limit > 0);
      check_one(&triggers, position(0), chain_to(position(1)));
      check_chain(&triggers, position(0), position(1));
      succeeded = true;
    }

    triggers_destroy(&triggers);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);
}

static void test32(void)
{
  /* Trigger read allocation failure */

  TriggersData source;
  populate(&source, 3);

  unsigned char buffer[Capacity];
  size_t const length = write_data(&source, false, &buffer);
  bool succeeded = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    TriggersData loaded;
    populate(&loaded, 0);
    Reader reader;
    assert(reader_mem_init(&reader, buffer, length));

    Fortify_SetNumAllocationsLimit(limit);
    SFError const err = triggers_read(&loaded, &reader);
    Fortify_SetNumAllocationsLimit(ULONG_MAX);

    if (SFError_fail(err))
    {
      assert(err.type == SFErrorType_NoMem);
    }
    else
    {
      assert(limit > 0);
      check_pattern(&loaded, 3);
      succeeded = true;
    }

    reader_destroy(&reader);
    triggers_destroy(&loaded);

    if (succeeded)
    {
      break;
    }
  }

  assert(succeeded);

  triggers_destroy(&source);
}

#endif

void Triggers_tests(void)
{
  /* Triggers */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial trigger count", test1 },
    { "Initial trigger locations", test2 },
    { "Added trigger values", test3 },
    { "Untouched trigger locations", test4 },
    { "Coincident triggers", test5 },
    { "Trigger coordinate wrapping", test6 },
    { "Spatial trigger iteration", test7 },
    { "Iteration without parameters", test8 },
    { "Wrapped trigger area", test9 },
    { "Trigger deletion during iteration", test10 },
    { "Trigger capacity", test11 },
    { "Maximum losses setter", test12 },
    { "Chain with implicit destination", test13 },
    { "Chain with existing destination", test14 },
    { "Dummy destination replacement", test15 },
    { "Shared chain destination", test16 },
    { "Chain deletion cleanup", test17 },
    { "Chain iterator deletion", test18 },
    { "Deletion preserves dependent chain", test19 },
    { "Self-referencing chain", test20 },
    { "Chain capacity rollback", test21 },
    { "Trigger round trip", test22 },
    { "Chain round trip", test23 },
    { "Trigger truncation", test24 },
    { "Corrupt trigger data", test25 },
    { "Trigger stream position", test26 },
    { "Maximum losses round trip", test27 },
    { "Trigger output failure", test28 },
#ifdef FORTIFY
    { "Trigger initial allocation failure", test29 },
    { "Trigger add allocation failure", test30 },
    { "Chain allocation rollback", test31 },
    { "Trigger read allocation failure", test32 },
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
