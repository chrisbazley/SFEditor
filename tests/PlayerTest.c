/* SFEditor test: player equipment.
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
#include "../Player.h"
#include "../PlayerData.h"

enum { RecordSize = 16 };

typedef enum
{
  PlayerField_AllMustMatch,
  PlayerField_Coords,
  PlayerField_Direction,
  PlayerField_ShipType,
  PlayerField_EquipEnabled,
  PlayerField_LaserType,
  PlayerField_Engine,
  PlayerField_Control,
  PlayerField_Shields,
  PlayerField_ATA,
  PlayerField_ATG,
  PlayerField_Mines,
  PlayerField_Bombs,
  PlayerField_MegaLaser,
  PlayerField_MultiATA,
}
PlayerField;

static void check_fields(PlayerData const *actual,
  PlayerData const *expected, PlayerField except)
{
  if (except != PlayerField_Coords)
  {
    assert(actual->coords.x == expected->coords.x);
    assert(actual->coords.y == expected->coords.y);
    assert(actual->coords.z == expected->coords.z);
  }
  if (except != PlayerField_Direction)
  {
    assert(actual->direction == expected->direction);
  }
  if (except != PlayerField_ShipType)
  {
    assert(player_get_ship_type(actual) == expected->ship_type);
  }
  if (except != PlayerField_EquipEnabled)
  {
    assert(player_get_equip_enabled(actual) == expected->equip_enabled);
  }
  if (except != PlayerField_LaserType)
  {
    assert(player_get_laser_type(actual) == expected->laser_type);
  }
  if (except != PlayerField_Engine)
  {
    assert(player_get_engine(actual) == expected->engine);
  }
  if (except != PlayerField_Control)
  {
    assert(player_get_control(actual) == expected->control);
  }
  if (except != PlayerField_Shields)
  {
    assert(player_get_shields(actual) == expected->shields);
  }
  if (except != PlayerField_ATA)
  {
    assert(player_get_ata(actual) == expected->ata);
  }
  if (except != PlayerField_ATG)
  {
    assert(player_get_atg(actual) == expected->atg);
  }
  if (except != PlayerField_Mines)
  {
    assert(player_get_mines(actual) == expected->mines);
  }
  if (except != PlayerField_Bombs)
  {
    assert(player_get_bombs(actual) == expected->bombs);
  }
  if (except != PlayerField_MegaLaser)
  {
    assert(player_get_mega_laser(actual) == expected->mega_laser);
  }
  if (except != PlayerField_MultiATA)
  {
    assert(player_get_multi_ata(actual) == expected->multi_ata);
  }
}

static PlayerData const initial = {
  .state = PlayerDataState_Write, .coords = {.x = 0, .y = 1, .z = 2},
  .direction = ShipDirection_NE, .docked_in.ship = NULL,
  .ship_type = ShipType_Player, .equip_enabled = false,
  .laser_type = 5, .engine = 6, .control = 7, .shields = 8,
  .ata = 9, .atg = 10, .mines = 11, .bombs = 12, .mega_laser = 13, .multi_ata = 14,
};

static unsigned char initial_byte(size_t index)
{
  return (unsigned char)(index == 4 || index == 15 ? 0 : index);
}

static void check_saved(PlayerData const *player, size_t offset, int value)
{
  unsigned char data[RecordSize];
  Writer writer;
  assert(writer_mem_init(&writer, data, sizeof(data)));
  player_write(player, &writer);
  assert(writer_destroy(&writer) == sizeof(data));
  for (size_t i = 0; i < sizeof(data); ++i)
  {
    int const expected = i == offset ? value : initial_byte(i);
    assert(data[i] == expected);
  }
}

static void make_record(unsigned char data[RecordSize])
{
  for (size_t i = 0; i < RecordSize; ++i)
  {
    data[i] = initial_byte(i);
  }
}

static SFError read_data(PlayerData *value, unsigned char const *data, size_t size)
{
  Reader reader;
  assert(reader_mem_init(&reader, data, size));
  SFError const err = player_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static void test1(void)
{
  /* Ship type */

  for (int value = 0; value <= ShipType_Fighter4; ++value)
  {
    PlayerData player = initial;
    player_set_ship_type(&player, (ShipType)value);
    assert(player_get_ship_type(&player) == (ShipType)value);
    check_fields(&player, &initial, PlayerField_ShipType);
    check_saved(&player, 15, value);
  }
}

static void test2(void)
{
  /* Equipment flag */

  for (int value = 0; value <= 1; ++value)
  {
    PlayerData player = initial;
    player_set_equip_enabled(&player, value != 0);
    assert(player_get_equip_enabled(&player) == value);
    check_fields(&player, &initial, PlayerField_EquipEnabled);
    check_saved(&player, 4, value);
  }
}

static void test3(void)
{
  /* Laser type */

  for (int value = 0; value <= PlayerMaxLaserType; ++value)
  {
    PlayerData player = initial;
    player_set_laser_type(&player, value);
    assert(player_get_laser_type(&player) == value);
    check_fields(&player, &initial, PlayerField_LaserType);
    check_saved(&player, 5, value);
  }
}

static void test4(void)
{
  /* Engine */

  for (int value = 0; value <= PlayerMaxEngine; ++value)
  {
    PlayerData player = initial;
    player_set_engine(&player, value);
    assert(player_get_engine(&player) == value);
    check_fields(&player, &initial, PlayerField_Engine);
    check_saved(&player, 6, value);
  }
}

static void test5(void)
{
  /* Control */

  for (int value = 0; value <= PlayerMaxControl; ++value)
  {
    PlayerData player = initial;
    player_set_control(&player, value);
    assert(player_get_control(&player) == value);
    check_fields(&player, &initial, PlayerField_Control);
    check_saved(&player, 7, value);
  }
}

static void test6(void)
{
  /* Shields */

  for (int value = 0; value <= PlayerMaxShields; ++value)
  {
    PlayerData player = initial;
    player_set_shields(&player, value);
    assert(player_get_shields(&player) == value);
    check_fields(&player, &initial, PlayerField_Shields);
    check_saved(&player, 8, value);
  }
}

static void test7(void)
{
  /* Air missiles */

  for (int value = 0; value <= PlayerMaxATA; ++value)
  {
    PlayerData player = initial;
    player_set_ata(&player, value);
    assert(player_get_ata(&player) == value);
    check_fields(&player, &initial, PlayerField_ATA);
    check_saved(&player, 9, value);
  }
}

static void test8(void)
{
  /* Ground missiles */

  for (int value = 0; value <= PlayerMaxATG; ++value)
  {
    PlayerData player = initial;
    player_set_atg(&player, value);
    assert(player_get_atg(&player) == value);
    check_fields(&player, &initial, PlayerField_ATG);
    check_saved(&player, 10, value);
  }
}

static void test9(void)
{
  /* Mines */

  for (int value = 0; value <= PlayerMaxMines; ++value)
  {
    PlayerData player = initial;
    player_set_mines(&player, value);
    assert(player_get_mines(&player) == value);
    check_fields(&player, &initial, PlayerField_Mines);
    check_saved(&player, 11, value);
  }
}

static void test10(void)
{
  /* Bombs */

  for (int value = 0; value <= PlayerMaxBombs; ++value)
  {
    PlayerData player = initial;
    player_set_bombs(&player, value);
    assert(player_get_bombs(&player) == value);
    check_fields(&player, &initial, PlayerField_Bombs);
    check_saved(&player, 12, value);
  }
}

static void test11(void)
{
  /* Mega laser */

  for (int value = 0; value <= PlayerMaxMegaLaser; ++value)
  {
    PlayerData player = initial;
    player_set_mega_laser(&player, value);
    assert(player_get_mega_laser(&player) == value);
    check_fields(&player, &initial, PlayerField_MegaLaser);
    check_saved(&player, 13, value);
  }
}

static void test12(void)
{
  /* Multi missiles */

  for (int value = 0; value <= PlayerMaxMultiATA; ++value)
  {
    PlayerData player = initial;
    player_set_multi_ata(&player, value);
    assert(player_get_multi_ata(&player) == value);
    check_fields(&player, &initial, PlayerField_MultiATA);
    check_saved(&player, 14, value);
  }
}

static void test13(void)
{
  /* Player round trip */

  for (unsigned v = 0; v < 256; ++v)
  {
    PlayerData player = initial, loaded = initial;
    player.coords = (CoarsePoint3d){.x = (CoarseCoord)v, .y = (CoarseCoord)(255 - v),
                                   .z = (CoarseCoord)(v ^ 85)};
    player.direction = (ShipDirection)(v % 8);
    player_set_ship_type(&player, (ShipType)(v % 5));
    player_set_equip_enabled(&player, v % 2 != 0);
    player_set_laser_type(&player, (int)(v % 8));
    player_set_engine(&player, (int)(v % 17));
    player_set_control(&player, (int)((v + 3) % 17));
    player_set_shields(&player, (int)(v % 13));
    player_set_ata(&player, (int)(v));
    player_set_atg(&player, (int)(255 - v));
    player_set_mines(&player, (int)(v ^ 85));
    player_set_bombs(&player, (int)(v ^ 170));
    player_set_mega_laser(&player, (int)((v + 7) % 256));
    player_set_multi_ata(&player, (int)((v + 13) % 256));
    unsigned char data[RecordSize];
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    player_write(&player, &writer);
    assert(writer_destroy(&writer) == sizeof(data));
    assert(!SFError_fail(read_data(&loaded, data, sizeof(data))));
    check_fields(&loaded, &player, PlayerField_AllMustMatch);
  }
}

static void test14(void)
{
  /* Player truncation */

  unsigned char data[RecordSize];
  make_record(data);
  for (size_t length = 0; length < sizeof(data); ++length)
  {
    PlayerData player = initial;
    assert(read_data(&player, data, length).type == SFErrorType_ReadFail);
  }
}

static void test15(void)
{
  /* Invalid direction */

  unsigned char data[RecordSize];
  make_record(data);
  data[3] = ShipDirection_SW + 1;
  PlayerData player = initial;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerDir);
  data[3] = 255;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerDir);
}

static void test16(void)
{
  /* Invalid equipment flag */

  unsigned char data[RecordSize];
  make_record(data);
  data[4] = 2;
  PlayerData player = initial;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadEnableEquip);
  data[4] = 255;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadEnableEquip);
}

static void test17(void)
{
  /* Invalid laser type */

  unsigned char data[RecordSize];
  make_record(data);
  data[5] = PlayerMaxLaserType + 1;
  PlayerData player = initial;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerLaserType);
  data[5] = 255;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerLaserType);
}

static void test18(void)
{
  /* Invalid engine */

  unsigned char data[RecordSize];
  make_record(data);
  data[6] = PlayerMaxEngine + 1;
  PlayerData player = initial;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerEngine);
  data[6] = 255;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerEngine);
}

static void test19(void)
{
  /* Invalid control */

  unsigned char data[RecordSize];
  make_record(data);
  data[7] = PlayerMaxControl + 1;
  PlayerData player = initial;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerControl);
  data[7] = 255;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerControl);
}

static void test20(void)
{
  /* Invalid shields */

  unsigned char data[RecordSize];
  make_record(data);
  data[8] = PlayerMaxShields + 1;
  PlayerData player = initial;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerShields);
  data[8] = 255;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadPlayerShields);
}

static void test21(void)
{
  /* Invalid ship type */

  unsigned char data[RecordSize];
  make_record(data);
  data[15] = ShipType_Fighter4 + 1;
  PlayerData player = initial;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadSpecialType);
  data[15] = 255;
  assert(read_data(&player, data, sizeof(data)).type == SFErrorType_BadSpecialType);
}

void Player_tests(void)
{
  /* Player */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Ship type", test1 },
    { "Equipment flag", test2 },
    { "Laser type", test3 },
    { "Engine", test4 },
    { "Control", test5 },
    { "Shields", test6 },
    { "Air missiles", test7 },
    { "Ground missiles", test8 },
    { "Mines", test9 },
    { "Bombs", test10 },
    { "Mega laser", test11 },
    { "Multi missiles", test12 },
    { "Player round trip", test13 },
    { "Player truncation", test14 },
    { "Invalid direction", test15 },
    { "Invalid equipment flag", test16 },
    { "Invalid laser type", test17 },
    { "Invalid engine", test18 },
    { "Invalid control", test19 },
    { "Invalid shields", test20 },
    { "Invalid ship type", test21 },
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
