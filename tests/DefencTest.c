/* SFEditor test: ground defence parameters.
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
#include "../Defenc.h"
#include "../DefencData.h"

enum { Capacity = 64 };

typedef enum
{
  DefenceField_AllMustMatch,
  DefenceField_Timer,
  DefenceField_ShipType,
  DefenceField_FireProb,
  DefenceField_LaserType,
  DefenceField_ShipsPerHangar,
  DefenceField_ShipProb,
}
DefenceField;

static void check_fields(DefencesData const *actual,
  DefencesData const *expected, DefenceField except)
{
  if (except != DefenceField_Timer)
  {
    assert(defences_get_timer(actual) == expected->timer);
  }
  if (except != DefenceField_ShipType)
  {
    assert(defences_get_ship_type(actual) == expected->ship_type);
  }
  if (except != DefenceField_FireProb)
  {
    assert(defences_get_fire_prob(actual) == expected->fire_prob);
  }
  if (except != DefenceField_LaserType)
  {
    assert(defences_get_laser_type(actual) == expected->laser_type);
  }
  if (except != DefenceField_ShipsPerHangar)
  {
    assert(defences_get_ships_per_hangar(actual) == expected->ships_per_hangar);
  }
  if (except != DefenceField_ShipProb)
  {
    assert(defences_get_ship_prob(actual) == expected->ship_prob);
  }
}

static DefencesData const initial = {
  .timer = 100, .ship_type = ShipType_Fighter2, .fire_prob = 37,
  .laser_type = 5, .ships_per_hangar = 7, .ship_prob = 201,
};

static size_t write_data(DefencesData const *value, unsigned char (*buffer)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  defences_write(value, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise any space skipped at the end, without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(DefencesData *value,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = defences_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static SFErrorType const expected_errors[] = {
  SFErrorType_BadDefencesTimer,
  SFErrorType_BadDefencesLaserType,
  SFErrorType_BadDefencesShipType,
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
  /* Activation timer */

  static int const values[] = {0, 1, INT32_MAX};
  for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
  {
    int const value = values[i];
    DefencesData d = initial;
    defences_set_timer(&d, value);
    assert(defences_get_timer(&d) == value);
    check_fields(&d, &initial, DefenceField_Timer);
  }
}

static void test2(void)
{
  /* Launch ship type */

  for (int value = ShipType_Fighter1; value <= ShipType_Fighter4; ++value)
  {
    DefencesData d = initial;
    defences_set_ship_type(&d, (ShipType)value);
    assert(defences_get_ship_type(&d) == (ShipType)value);
    check_fields(&d, &initial, DefenceField_ShipType);
  }
}

static void test3(void)
{
  /* Fire probability */

  for (int value = 0; value <= DefencesMaxProb; ++value)
  {
    DefencesData d = initial;
    defences_set_fire_prob(&d, value);
    assert(defences_get_fire_prob(&d) == value);
    check_fields(&d, &initial, DefenceField_FireProb);
  }
}

static void test4(void)
{
  /* Laser type */

  for (int value = 0; value <= DefencesMaxLaserType; ++value)
  {
    DefencesData d = initial;
    defences_set_laser_type(&d, value);
    assert(defences_get_laser_type(&d) == value);
    check_fields(&d, &initial, DefenceField_LaserType);
  }
}

static void test5(void)
{
  /* Ships per hangar */

  for (int value = 0; value <= DefencesMaxShipsPerHangar; ++value)
  {
    DefencesData d = initial;
    defences_set_ships_per_hangar(&d, value);
    assert(defences_get_ships_per_hangar(&d) == value);
    check_fields(&d, &initial, DefenceField_ShipsPerHangar);
  }
}

static void test6(void)
{
  /* Launch probability */

  for (int value = 0; value <= DefencesMaxProb; ++value)
  {
    DefencesData d = initial;
    defences_set_ship_prob(&d, value);
    assert(defences_get_ship_prob(&d) == value);
    check_fields(&d, &initial, DefenceField_ShipProb);
  }
}

static void test7(void)
{
  /* Defence round trip */

  for (unsigned v = 0; v < 256; ++v)
  {
    DefencesData d = initial, loaded;
    int32_t const timer = v == 0 ? 0 : INT32_MAX - (int32_t)(UINT32_C(0x01010101) * (v % 128));
    defences_set_timer(&d, timer);
    defences_set_ship_type(&d, (ShipType)(ShipType_Fighter1 + v % 4));
    defences_set_fire_prob(&d, (int)v);
    defences_set_laser_type(&d, (int)(v % 9));
    defences_set_ships_per_hangar(&d, (int)(v % 16));
    defences_set_ship_prob(&d, (int)(255 - v));
    unsigned char buffer[Capacity];
    size_t const size = write_data(&d, &buffer);
    assert(!SFError_fail(read_data(&loaded, &buffer, size)));
    check_fields(&loaded, &d, DefenceField_AllMustMatch);
  }
}

static void test8(void)
{
  /* Corrupt defence data */

  unsigned char buffer[Capacity];
  size_t const size = write_data(&initial, &buffer);
  bool seen[ErrorCount] = {false};
  for (size_t offset = 0; offset < size; ++offset)
  {
    unsigned char const saved = buffer[offset];
    for (unsigned byte = 0; byte < 256; ++byte)
    {
      buffer[offset] = (unsigned char)byte;
      DefencesData loaded;
      note_error(read_data(&loaded, &buffer, size).type, &seen);
    }
    buffer[offset] = saved;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    assert(seen[i]);
  }
}

static void test9(void)
{
  /* Defence truncation */

  unsigned char buffer[Capacity];
  size_t const size = write_data(&initial, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    DefencesData loaded;
    SFError const err = read_data(&loaded, &buffer, length);
    if (length == 0)
    {
      assert(SFError_fail(err));
    }
    if (!SFError_fail(err))
    {
      check_fields(&loaded, &initial, DefenceField_AllMustMatch);
    }
  }
}

void Defenc_tests(void)
{
  /* Defenc */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Activation timer", test1 },
    { "Launch ship type", test2 },
    { "Fire probability", test3 },
    { "Laser type", test4 },
    { "Ships per hangar", test5 },
    { "Launch probability", test6 },
    { "Defence round trip", test7 },
    { "Corrupt defence data", test8 },
    { "Defence truncation", test9 },
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
