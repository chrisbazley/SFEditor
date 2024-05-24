/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission fighters performance
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
#include "FPerf.h"
#include "FPerfData.h"

enum
{
  FighterPerformMinLaserType = 0,
  FighterPerformMaxLaserType = 7, /* 8 is never used for fighters */
  FighterPerformMinProb = -1,     /* P(n)=0.0 */
  FighterPerformMaxProb = 2047,   /* P(n)=1.0 */
  FighterPerformMinShields = 50,
  FighterPerformMaxShields = 1500,
  FighterPerformMinControl = 2, /* in easy mission 9 */
  FighterPerformMaxControl = 30,
  FighterPerformMinEngine = 10, /* in medium mission 9 */
  FighterPerformMaxEngine = 35000,
  FighterPerformPadding = 8,
};

SFError fighter_perform_read(FighterPerformData *const fighter,
  Reader *const reader)
{
  assert(fighter);

  for (ShipType t = ShipType_Fighter1; t <= ShipType_Fighter4; ++t)
  {
    DEBUGF("Reading fighter %d performance data at %ld\n", (int)t, reader_ftell(reader));
    int32_t laser_prob = 0;
    if (!reader_fread_int32(&laser_prob, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Fighter laser fire probability %" PRId32 "\n", laser_prob);

    if (laser_prob < FighterPerformMinProb ||
        laser_prob > FighterPerformMaxProb)
    {
      return SFERROR(BadLaserProb);
    }

    int32_t laser_type = 0;
    if (!reader_fread_int32(&laser_type, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Fighter laser type %" PRId32 "\n", laser_type);

    if (laser_type < FighterPerformMinLaserType ||
        laser_type > FighterPerformMaxLaserType)
    {
      return SFERROR(BadLaserType);
    }

    int32_t engine = 0;
    if (!reader_fread_int32(&engine, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Fighter engine power %" PRId32 "\n", engine);

    if (engine < FighterPerformMinEngine ||
        engine > FighterPerformMaxEngine)
    {
      return SFERROR(BadEngine);
    }

    int32_t control = 0;
    if (!reader_fread_int32(&control, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Fighter manoeuvrability %" PRId32 "\n", control);

    if (control < FighterPerformMinControl ||
        control > FighterPerformMaxControl)
    {
      return SFERROR(BadControl);
    }

    int32_t shields = 0;
    if (!reader_fread_int32(&shields, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Fighter shields %" PRId32 "\n", shields);

    if (shields < FighterPerformMinShields ||
        shields > FighterPerformMaxShields)
    {
      return SFERROR(BadShields);
    }

    int32_t missile_prob = 0;
    if (!reader_fread_int32(&missile_prob, reader))
    {
      return SFERROR(ReadFail);
    }
    DEBUGF("Fighter missile launch probability %" PRId32 "\n", missile_prob);

    if (missile_prob < FighterPerformMinProb ||
        missile_prob > FighterPerformMaxProb)
    {
      return SFERROR(BadMissileProb);
    }

    /* skip big ships' values */
    if (reader_fseek(reader, FighterPerformPadding, SEEK_CUR))
    {
      return SFERROR(BadSeek);
    }

    FighterPerform *const type = fighter_perform_get_ship(fighter, t);
    *type = (FighterPerform){
      .laser_prob = laser_prob,
      .laser_type = laser_type,
      .missile_prob = missile_prob,
      .shields = shields,
      .engine = engine,
      .control = control,
    };
  }

  DEBUGF("Finished reading fighter performance data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

void fighter_perform_write(FighterPerformData const *const fighter,
  Writer *const writer)
{
  assert(fighter);

  for (ShipType t = ShipType_Fighter1;
       t <= ShipType_Fighter4 && !writer_ferror(writer);
       ++t)
  {
    FighterPerform const *const type = &fighter->type[t - ShipType_Fighter1];

    assert(type->laser_prob >= FighterPerformMinProb);
    assert(type->laser_prob <= FighterPerformMaxProb);
    writer_fwrite_int32(type->laser_prob, writer);

    assert(type->laser_type >= FighterPerformMinLaserType);
    assert(type->laser_type <= FighterPerformMaxLaserType);
    writer_fwrite_int32(type->laser_type, writer);

    assert(type->engine >= FighterPerformMinEngine);
    assert(type->engine <= FighterPerformMaxEngine);
    writer_fwrite_int32(type->engine, writer);

    assert(type->control >= FighterPerformMinControl);
    assert(type->control <= FighterPerformMaxControl);
    writer_fwrite_int32(type->control, writer);

    assert(type->shields >= FighterPerformMinShields);
    assert(type->shields <= FighterPerformMaxShields);
    writer_fwrite_int32(type->shields, writer);

    assert(type->missile_prob >= FighterPerformMinProb);
    assert(type->missile_prob <= FighterPerformMaxProb);
    writer_fwrite_int32(type->missile_prob, writer);

    /* skip big ships' values */
    writer_fseek(writer, FighterPerformPadding, SEEK_CUR);
  }
  DEBUGF("Finished writing fighter performance data at %ld\n", writer_ftell(writer));
}

FighterPerform *fighter_perform_get_ship(FighterPerformData *const fighter, ShipType const type)
{
  assert(fighter);
  assert(type >= ShipType_Fighter1);
  assert(type <= ShipType_Fighter4);
  return &fighter->type[type - ShipType_Fighter1];
}

int fighter_perform_get_shields(FighterPerform const *const fighter)
{
  assert(fighter);
  return fighter->shields;
}

int fighter_perform_get_engine(FighterPerform const *const fighter)
{
  assert(fighter);
  return fighter->engine;
}

int fighter_perform_get_control(FighterPerform const *const fighter)
{
  assert(fighter);
  return fighter->control;
}

int fighter_perform_get_laser_prob(FighterPerform const *const fighter)
{
  assert(fighter);
  return fighter->laser_prob;
}

int fighter_perform_get_missile_prob(FighterPerform const *const fighter)
{
  assert(fighter);
  return fighter->missile_prob;
}

int fighter_perform_get_laser_type(FighterPerform const *const fighter)
{
  assert(fighter);
  return fighter->laser_type;
}

void fighter_perform_set_shields(FighterPerform *const fighter, int const shields)
{
  assert(fighter);
  assert(shields >= FighterPerformMinShields);
  assert(shields <= FighterPerformMaxShields);
  fighter->shields = shields;
}

void fighter_perform_set_engine(FighterPerform *const fighter, int const engine)
{
  assert(fighter);
  assert(engine >= FighterPerformMinEngine);
  assert(engine <= FighterPerformMaxEngine);
  fighter->engine = engine;
}

void fighter_perform_set_control(FighterPerform *const fighter, int const control)
{
  assert(fighter);
  assert(control >= FighterPerformMinControl);
  assert(control <= FighterPerformMaxControl);
  fighter->control = control;
}

void fighter_perform_set_laser_prob(FighterPerform *const fighter, int const laser_prob)
{
  assert(fighter);
  assert(laser_prob >= FighterPerformMinProb);
  assert(laser_prob <= FighterPerformMaxProb);
  fighter->laser_prob = laser_prob;
}

void fighter_perform_set_missile_prob(FighterPerform *const fighter, int const missile_prob)
{
  assert(fighter);
  assert(missile_prob >= FighterPerformMinProb);
  assert(missile_prob <= FighterPerformMaxProb);
  fighter->missile_prob = missile_prob;
}

void fighter_perform_set_laser_type(FighterPerform *const fighter, int const laser_type)
{
  assert(fighter);
  assert(laser_type >= FighterPerformMinLaserType);
  assert(laser_type <= FighterPerformMaxLaserType);
  fighter->laser_type = laser_type;
}
