/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Player's ship
 *  Copyright (C) 2020 Christopher Bazley
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public Licence as published by
 *  the Free Software Foundation; either version 2 of the Licence, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public Licence for more details.
 *
 *  You should have received a copy of the GNU General Public Licence
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"

#include "Ships.h"
#include "SFError.h"
#include "Player.h"
#include "PlayerData.h"

enum {
  PlayerMaxLaserType = 7,
  PlayerMaxControl = 16,
  PlayerMaxEngine = 16,
  PlayerMaxShields = 12,
  PlayerMaxATA = 255,
  PlayerMaxATG = 255,
  PlayerMaxMines = 255,
  PlayerMaxBombs = 255,
  PlayerMaxMegaLaser = 255,
  PlayerMaxMultiATA = 255,
  PlayerNotDocked = 255,
};

void player_init(PlayerData *const player)
{
  assert(player);
  *player = (PlayerData){.direction = ShipDirection_S,
                         .state = PlayerDataState_Write,
                         .docked_in.ship = NULL,
                         .ship_type = ShipType_Player};
}

SFError player_read(PlayerData *const player,
  Reader *const reader)
{
  assert(player);
  assert(player->state == PlayerDataState_Write);

  if (!CoarsePoint3d_read(&player->coords, reader))
  {
    return SFERROR(ReadFail);
  }

  int const direction = reader_fgetc(reader);
  if (direction == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (direction < ShipDirection_S || direction > ShipDirection_SW)
  {
    return SFERROR(BadPlayerDir);
  }
  player->direction = direction;

  int const equip_enabled = reader_fgetc(reader);
  if (equip_enabled == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (equip_enabled != 0 && equip_enabled != 1)
  {
    return SFERROR(BadEnableEquip);
  }
  player->equip_enabled = equip_enabled;

  int const laser_type = reader_fgetc(reader);
  if (laser_type == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (laser_type > PlayerMaxLaserType)
  {
    return SFERROR(BadPlayerLaserType);
  }
  player->laser_type = laser_type;

  int const engine = reader_fgetc(reader);
  if (engine == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (engine > PlayerMaxEngine)
  {
    return SFERROR(BadPlayerEngine);
  }
  player->engine = engine;

  int const control = reader_fgetc(reader);
  if (control == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (control > PlayerMaxEngine)
  {
    return SFERROR(BadPlayerControl);
  }
  player->control = control;

  int const shields = reader_fgetc(reader);
  if (shields == EOF)
  {
    return SFERROR(ReadFail);
  }
  if (shields > PlayerMaxShields)
  {
    return SFERROR(BadPlayerShields);
  }
  player->shields = shields;

  unsigned char counts[6] = {0};
  if (reader_fread(counts, sizeof(counts), 1, reader) != 1)
  {
    return SFERROR(ReadFail);
  }
  size_t i = 0;
  player->ata = counts[i++];
  player->atg = counts[i++];
  player->mines = counts[i++];

  player->bombs = counts[i++];
  player->mega_laser = counts[i++];
  player->multi_ata = counts[i++];
  assert(i == ARRAY_SIZE(counts));

  int const ship_type = reader_fgetc(reader);
  if (ship_type == EOF)
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Player's ship type %d\n", ship_type);
  if (ship_type < ShipType_Player || ship_type > ShipType_Fighter4)
  {
    return SFERROR(BadSpecialType);
  }
  player->ship_type = ship_type;

  DEBUGF("Finished reading player data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

SFError player_read_docked(PlayerData *const player,
  Reader *const reader)
{
  assert(player);
  assert(player->state == PlayerDataState_Write);

  int const start_docked = reader_fgetc(reader);
  if (start_docked == EOF)
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Start-docked %d\n", start_docked);
  player->docked_in.num = start_docked;
  player->state = PlayerDataState_PostRead;
  return SFERROR(OK);
}

SFError player_post_read(PlayerData *const player, ShipsData *const ships)
{
  assert(player);
  assert(player->state == PlayerDataState_PostRead);
  DEBUGF("Fixing up ship docked in %d\n", player->docked_in.num);
  if (player->docked_in.num == PlayerNotDocked)
  {
    player->docked_in.ship = NULL;
  }
  else
  {
    player->docked_in.ship = ship_from_index(ships, player->docked_in.num);
    if (player->docked_in.ship == NULL)
    {
      return SFERROR(BadStartDocked);
    }
  }
  DEBUGF("Player starts docked in %p\n", (void *)player->docked_in.ship);
  player->state = PlayerDataState_Write;
  return SFERROR(OK);
}

void player_write(PlayerData const *const player,
  Writer *const writer)
{
  assert(player);
  assert(player->state == PlayerDataState_Write);

  CoarsePoint3d_write(player->coords, writer);

  assert(player->direction >= ShipDirection_S);
  assert(player->direction <= ShipDirection_SW);
  writer_fputc(player->direction, writer);

  writer_fputc(player->equip_enabled, writer);

  assert(player->laser_type <= PlayerMaxLaserType);
  writer_fputc(player->laser_type, writer);

  assert(player->engine <= PlayerMaxEngine);
  writer_fputc(player->engine, writer);

  assert(player->control <= PlayerMaxControl);
  writer_fputc(player->control, writer);

  assert(player->shields <= PlayerMaxShields);
  writer_fputc(player->shields, writer);

  writer_fputc(player->ata, writer);
  writer_fputc(player->atg, writer);
  writer_fputc(player->mines, writer);
  writer_fputc(player->bombs, writer);
  writer_fputc(player->mega_laser, writer);
  writer_fputc(player->multi_ata, writer);

  assert(player->ship_type >= ShipType_Player);
  assert(player->ship_type <= ShipType_Fighter4);
  writer_fputc(player->ship_type, writer);

  DEBUGF("Finished writing player data at %ld\n", writer_ftell(writer));
}

void player_write_docked(PlayerData const *const player,
  Writer *const writer)
{
  assert(player);
  assert(player->state == PlayerDataState_Write);
  if (player->docked_in.ship == NULL)
  {
    writer_fputc(PlayerNotDocked, writer);
  }
  else
  {
    writer_fputc(ship_get_index(player->docked_in.ship), writer);
  }
}

ShipType player_get_ship_type(PlayerData const *const player)
{
  assert(player);
  assert(player->state == PlayerDataState_Write);
  return player->ship_type;
}

void player_set_ship_type(PlayerData *const player, ShipType ship_type)
{
  assert(player);
  assert(player->state == PlayerDataState_Write);
  assert(player->ship_type >= ShipType_Player);
  assert(player->ship_type <= ShipType_Fighter4);
  player->ship_type = ship_type;
}

bool player_get_equip_enabled(PlayerData const *player)
{
  assert(player);
  return player->equip_enabled;
}

void player_set_equip_enabled(PlayerData *player, bool enabled)
{
  assert(player);
  player->equip_enabled = enabled;
}

int player_get_laser_type(PlayerData const *player)
{
  assert(player);
  return player->laser_type;
}

void player_set_laser_type(PlayerData *player, int laser_type)
{
  assert(player);
  assert(laser_type >= 0);
  assert(laser_type <= PlayerMaxLaserType);
  player->laser_type = laser_type;
}

int player_get_engine(PlayerData const *player)
{
  assert(player);
  return player->engine;
}

void player_set_engine(PlayerData *player, int engine)
{
  assert(player);
  assert(engine >= 0);
  assert(engine <= PlayerMaxEngine);
  player->engine = engine;
}

int player_get_control(PlayerData const *player)
{
  assert(player);
  return player->control;
}

void player_set_control(PlayerData *player, int control)
{
  assert(player);
  assert(control >= 0);
  assert(control <= PlayerMaxControl);
  player->control = control;
}

int player_get_shields(PlayerData const *player)
{
  assert(player);
  return player->shields;
}

void player_set_shields(PlayerData *player, int shields)
{
  assert(player);
  assert(shields >= 0);
  assert(shields <= PlayerMaxShields);
  player->shields = shields;
}

int player_get_ata(PlayerData const *player)
{
  assert(player);
  return player->ata;
}

void player_set_ata(PlayerData *player, int ata)
{
  assert(player);
  assert(ata >= 0);
  assert(ata <= PlayerMaxATA);
  player->ata = ata;
}

int player_get_atg(PlayerData const *player)
{
  assert(player);
  return player->atg;
}

void player_set_atg(PlayerData *player, int atg)
{
  assert(player);
  assert(atg >= 0);
  assert(atg <= PlayerMaxATG);
  player->atg = atg;
}

int player_get_mines(PlayerData const *player)
{
  assert(player);
  return player->mines;
}

void player_set_mines(PlayerData *player, int mines)
{
  assert(player);
  assert(mines >= 0);
  assert(mines <= PlayerMaxMines);
  player->mines = mines;
}

int player_get_bombs(PlayerData const *player)
{
  assert(player);
  return player->bombs;
}

void player_set_bombs(PlayerData *player, int bombs)
{
  assert(player);
  assert(bombs >= 0);
  assert(bombs <= PlayerMaxBombs);
  player->bombs = bombs;
}

int player_get_mega_laser(PlayerData const *player)
{
  assert(player);
  return player->mega_laser;
}

void player_set_mega_laser(PlayerData *player, int mega_laser)
{
  assert(player);
  assert(mega_laser >= 0);
  assert(mega_laser <= PlayerMaxMegaLaser);
  player->mega_laser = mega_laser;
}

int player_get_multi_ata(PlayerData const *player)
{
  assert(player);
  return player->multi_ata;
}

void player_set_multi_ata(PlayerData *player, int multi_ata)
{
  assert(player);
  assert(multi_ata >= 0);
  assert(multi_ata <= PlayerMaxMultiATA);
  player->multi_ata = multi_ata;
}
