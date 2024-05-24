/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission big ships performance data
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

#include <inttypes.h>

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"

#include "Ships.h"
#include "SFError.h"
#include "BPerf.h"
#include "BPerfData.h"

enum
{
  BigPerformMinLaserType = 0,
  BigPerformMaxLaserType = 8,
  BigPerformMinProb = -1,      /* P(n)=0.0 */
  BigPerformMaxProb = 2047,    /* P(n)=1.0 */
  BigPerformMinShields = 0,     /* surprisingly common */
  BigPerformMaxShields = 10000, /* excluding remote generator */
  BigAllowBadMinShips = -1, /* surprisingly common */
  BigPerformMinShips = 0,
  BigPerformMaxShips = 10,
  BigPerformPadding = 8,
  BigRemoteShieldGenerator = 999999,
};

SFError big_perform_read(BigPerformData *const big,
  Reader *const reader)
{
  assert(big);

  for (ShipType t = ShipType_Big1; t <= ShipType_Big3; ++t)
  {
    DEBUGF("Reading ship %d performance data at %ld\n", (int)t, reader_ftell(reader));
    int32_t laser_prob = 0;
    if (!reader_fread_int32(&laser_prob, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Big ship laser fire probability %" PRId32 "\n", laser_prob);

    if (laser_prob < BigPerformMinProb ||
        laser_prob > BigPerformMaxProb)
    {
      return SFERROR(BadLaserProb);
    }

    int32_t laser_type = 0;
    if (!reader_fread_int32(&laser_type, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Big ship laser type %" PRId32 "\n", laser_type);

    if (laser_type < BigPerformMinLaserType ||
        laser_type > BigPerformMaxLaserType)
    {
      return SFERROR(BadLaserType);
    }

    /* skip fighters' values */
    if (reader_fseek(reader, BigPerformPadding, SEEK_CUR))
    {
      return SFERROR(BadSeek);
    }

    int32_t shields = 0;
    if (!reader_fread_int32(&shields, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Big ship shields %" PRId32 "\n", shields);

    if (shields < BigPerformMinShields ||
        (shields > BigPerformMaxShields && shields != BigRemoteShieldGenerator))
    {
      return SFERROR(BadShields);
    }

    bool remote_shield = false;
    if (shields == BigRemoteShieldGenerator)
    {
      shields = BigPerformMaxShields;
      remote_shield = true;
    }

    int32_t missile_prob = 0;
    if (!reader_fread_int32(&missile_prob, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Big ship missile launch probability %" PRId32 "\n", missile_prob);

    if (missile_prob < BigPerformMinProb ||
        missile_prob > BigPerformMaxProb)
    {
      return SFERROR(BadMissileProb);
    }

    int32_t ship_prob = 0;
    if (!reader_fread_int32(&ship_prob, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Big ship fighter launch probability %" PRId32 "\n", ship_prob);

    if (ship_prob < BigPerformMinProb ||
        ship_prob > BigPerformMaxProb)
    {
      return SFERROR(BadShipProb);
    }

    int32_t ship_count = 0;
    if (!reader_fread_int32(&ship_count, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Big ship hangar capacity %" PRId32 "\n", ship_count);

    if (ship_count == BigAllowBadMinShips)
    {
      ship_count = BigPerformMinShips;
    }
    else if (ship_count < BigPerformMinShips ||
             ship_count > BigPerformMaxShips)
    {
      return SFERROR(BadNumShips);
    }

    BigPerform *const type = big_perform_get_ship(big, t);
    *type = (BigPerform){
      .laser_prob = laser_prob,
      .laser_type = laser_type,
      .missile_prob = missile_prob,
      .shields = shields,
      .ship_prob = ship_prob,
      .ship_count = ship_count,
      .remote_shield = remote_shield,
    };
  }

  DEBUGF("Finished reading ship performance data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

void big_perform_write(BigPerformData const *const big,
  Writer *const writer)
{
  assert(big);

  for (ShipType t = ShipType_Big1;
       t <= ShipType_Big3 && !writer_ferror(writer);
       ++t)
  {
    BigPerform const *const type = &big->type[t - ShipType_Big1];

    assert(type->laser_prob >= BigPerformMinProb);
    assert(type->laser_prob <= BigPerformMaxProb);
    writer_fwrite_int32(type->laser_prob, writer);

    assert(type->laser_type >= BigPerformMinLaserType);
    assert(type->laser_type <= BigPerformMaxLaserType);
    writer_fwrite_int32(type->laser_type, writer);

    /* skip fighters' values */
    writer_fseek(writer, BigPerformPadding, SEEK_CUR);

    int32_t shields = type->shields;
    assert(shields >= BigPerformMinShields);
    assert(shields <= BigPerformMaxShields);
    if (type->remote_shield)
    {
      shields = BigRemoteShieldGenerator;
    }
    writer_fwrite_int32(shields, writer);

    assert(type->missile_prob >= BigPerformMinProb);
    assert(type->missile_prob <= BigPerformMaxProb);
    writer_fwrite_int32(type->missile_prob, writer);

    assert(type->ship_prob >= BigPerformMinProb);
    assert(type->ship_prob <= BigPerformMaxProb);
    writer_fwrite_int32(type->ship_prob, writer);

    assert(type->ship_count >= BigPerformMinShips);
    assert(type->ship_count <= BigPerformMaxShips);
    writer_fwrite_int32(type->ship_count, writer);
  }
  DEBUGF("Finished writing ship performance data at %ld\n", writer_ftell(writer));
}

BigPerform *big_perform_get_ship(BigPerformData *const big, ShipType const type)
{
  assert(big);
  assert(type >= ShipType_Big1);
  assert(type <= ShipType_Big3);
  return &big->type[type - ShipType_Big1];
}

int big_perform_get_shields(BigPerform const *const big)
{
  assert(big);
  return big->shields;
}

bool big_perform_has_remote_shield(BigPerform const *const big)
{
  assert(big);
  return big->remote_shield;
}

int big_perform_get_ship_prob(BigPerform const *const big)
{
  assert(big);
  return big->ship_prob;
}

int big_perform_get_ship_count(BigPerform const *const big)
{
  assert(big);
  return big->ship_count;
}

int big_perform_get_laser_prob(BigPerform const *const big)
{
  assert(big);
  return big->laser_prob;
}

int big_perform_get_missile_prob(BigPerform const *const big)
{
  assert(big);
  return big->missile_prob;
}

int big_perform_get_laser_type(BigPerform const *const big)
{
  assert(big);
  return big->laser_type;
}

void big_perform_set_shields(BigPerform *const big, int const shields)
{
  assert(big);
  assert(shields >= BigPerformMinShields);
  assert(shields <= BigPerformMaxShields);
  big->shields = shields;
  big->remote_shield = false;
}

void big_perform_set_remote_shield(BigPerform *const big)
{
  assert(big);
  big->remote_shield = true;
}

void big_perform_set_ship_prob(BigPerform *const big, int const ship_prob)
{
  assert(big);
  assert(ship_prob >= BigPerformMinProb);
  assert(ship_prob <= BigPerformMaxProb);
  big->ship_prob = ship_prob;
}

void big_perform_set_ship_count(BigPerform *const big, int const ship_count)
{
  assert(big);
  assert(ship_count >= BigPerformMinShips);
  assert(ship_count <= BigPerformMaxShips);
  big->ship_count = ship_count;
}

void big_perform_set_laser_prob(BigPerform *const big, int const laser_prob)
{
  assert(big);
  assert(laser_prob >= BigPerformMinProb);
  assert(laser_prob <= BigPerformMaxProb);
  big->laser_prob = laser_prob;
}

void big_perform_set_missile_prob(BigPerform *const big, int const missile_prob)
{
  assert(big);
  assert(missile_prob >= BigPerformMinProb);
  assert(missile_prob <= BigPerformMaxProb);
  big->missile_prob = missile_prob;
}

void big_perform_set_laser_type(BigPerform *const big, int const laser_type)
{
  assert(big);
  assert(laser_type >= BigPerformMinLaserType);
  assert(laser_type <= BigPerformMaxLaserType);
  big->laser_type = laser_type;
}
