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

static unsigned char const record[8] = {100, 0, 0, 0, 37, 5, 0x72, 201};

static SFError read_data(DefencesData *value, unsigned char const *data, size_t size)
{
  Reader reader;
  assert(reader_mem_init(&reader, data, size));
  SFError const err = defences_read(value, &reader);
  reader_destroy(&reader);
  return err;
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
    unsigned char data[8];
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    defences_write(&d, &writer);
    assert(writer_destroy(&writer) == sizeof(data));
    for (size_t byte = 0; byte < 4; ++byte)
    {
      assert(data[byte] == (((uint32_t)timer >> (8 * byte)) & 255));
    }
    assert(data[4] == v);
    assert(data[5] == v % 9);
    assert(data[6] == (v % 16) * 16 + ShipType_Fighter1 + v % 4);
    assert(data[7] == 255 - v);
    assert(!SFError_fail(read_data(&loaded, data, sizeof(data))));
    check_fields(&loaded, &d, DefenceField_AllMustMatch);
  }
}

static void test8(void)
{
  /* Defence truncation */

  for (size_t length = 0; length < sizeof(record); ++length)
  {
    DefencesData d = initial;
    assert(read_data(&d, record, length).type == SFErrorType_ReadFail);
  }
}

static void test9(void)
{
  /* Invalid activation timer */

  unsigned char data[sizeof(record)];
  memcpy(data, record, sizeof(data));
  Writer writer;
  assert(writer_mem_init(&writer, data, 4));
  writer_fwrite_int32(-1, &writer);
  assert(writer_destroy(&writer) == 4);
  DefencesData d = initial;
  assert(read_data(&d, data, sizeof(data)).type == SFErrorType_BadDefencesTimer);
}

static void test10(void)
{
  /* Invalid defence laser */

  unsigned char data[sizeof(record)];
  memcpy(data, record, sizeof(data));
  DefencesData d = initial;
  data[5] = DefencesMaxLaserType + 1;
  assert(read_data(&d, data, sizeof(data)).type == SFErrorType_BadDefencesLaserType);
  data[5] = 255;
  assert(read_data(&d, data, sizeof(data)).type == SFErrorType_BadDefencesLaserType);
}

static void test11(void)
{
  /* Invalid launch ship */

  unsigned char data[sizeof(record)];
  memcpy(data, record, sizeof(data));
  DefencesData d = initial;
  data[6] = ShipType_Fighter4 + 1;
  assert(read_data(&d, data, sizeof(data)).type == SFErrorType_BadDefencesShipType);
  data[6] = 15;
  assert(read_data(&d, data, sizeof(data)).type == SFErrorType_BadDefencesShipType);
}

static void test12(void)
{
  /* Player ship compatibility */

  unsigned char data[sizeof(record)];
  memcpy(data, record, sizeof(data));
  data[6] = 0x70;
  DefencesData d = initial;
  assert(!SFError_fail(read_data(&d, data, sizeof(data))));
  assert(defences_get_ship_type(&d) == ShipType_Fighter1);
  check_fields(&d, &initial, DefenceField_ShipType);
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
    { "Defence truncation", test8 },
    { "Invalid activation timer", test9 },
    { "Invalid defence laser", test10 },
    { "Invalid launch ship", test11 },
    { "Player ship compatibility", test12 },
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
