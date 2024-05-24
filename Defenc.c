/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission ground defences
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
#include "Defenc.h"
#include "DefencData.h"

enum {
  DefencesMinProb = 0,
  DefencesMaxProb = 255,
  DefencesMinLaserType = 0,
  DefencesMaxLaserType = 8,
  DefencesShipTypeMask = 0xf,
  DefencesShipTypeShift = 0,
  DefencesShipsPerHangarMask = 0xf0,
  DefencesShipsPerHangarShift = 4,
};

SFError defences_read(DefencesData *const defences,
  Reader *const reader)
{
  assert(defences);

  int32_t timer = 0;
  if (!reader_fread_int32(&timer, reader))
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Defences activation timer %" PRId32 "\n", timer);

  if (timer < 0)
  {
    return SFERROR(BadDefencesTimer);
  }

  int const fire_prob = reader_fgetc(reader);
  if (fire_prob == EOF)
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Defences fire probability %d\n", fire_prob);

  assert(fire_prob >= DefencesMinProb);
  assert(fire_prob <= DefencesMaxProb);

  int const laser_type = reader_fgetc(reader);
  if (laser_type == EOF)
  {
    return SFERROR(ReadFail);
  }
  DEBUGF("Defences fire laser type %d\n", laser_type);

  if ((laser_type < DefencesMinLaserType) ||
      (laser_type > DefencesMaxLaserType))
  {
    return SFERROR(BadDefencesLaserType);
  }

  int const ship_info = reader_fgetc(reader);
  if (ship_info == EOF)
  {
    return SFERROR(ReadFail);
  }

  int ship_type = (ship_info & DefencesShipTypeMask) >>
    DefencesShipTypeShift;
  DEBUGF("Defences launch ship type %d\n", ship_type);
  if ((ship_type < ShipType_Player) ||
      (ship_type > ShipType_Fighter4))
  {
    return SFERROR(BadDefencesShipType);
  }

  if (ship_type == ShipType_Player)
  {
    ship_type = ShipType_Fighter1; /* stop Tim from adding ship 0 */
  }

  int const ships_per_hangar = (ship_info & DefencesShipsPerHangarMask) >>
    DefencesShipsPerHangarShift;
  DEBUGF("Defences have %d ships per hangar\n", ships_per_hangar);

  int const ship_prob = reader_fgetc(reader);
  DEBUGF("Defences ship launch probability %d\n", ship_prob);

  assert(ship_prob >= DefencesMinProb);
  assert(ship_prob <= DefencesMaxProb);

  *defences = (DefencesData){
    .timer = timer,
    .fire_prob = fire_prob,
    .laser_type = laser_type,
    .ship_type = ship_type,
    .ships_per_hangar = ships_per_hangar,
    .ship_prob = ship_prob
  };
  DEBUGF("Finished reading defences data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

void defences_write(DefencesData const *const defences,
  Writer *const writer)
{
  assert(defences);

  writer_fwrite_int32(defences->timer, writer);
  writer_fputc(defences->fire_prob, writer);

  assert(defences->laser_type >= DefencesMinLaserType);
  assert(defences->laser_type <= DefencesMaxLaserType);
  writer_fputc(defences->laser_type, writer);

  assert(defences->ship_type >= ShipType_Fighter1);
  assert(defences->ship_type <= ShipType_Fighter4);

  assert(defences->ships_per_hangar <=
         (DefencesShipsPerHangarMask >> DefencesShipsPerHangarShift));

  int ship_info = (defences->ship_type << DefencesShipTypeShift) &
                  DefencesShipTypeMask;

  ship_info |= (defences->ships_per_hangar << DefencesShipsPerHangarShift) &
               DefencesShipsPerHangarMask;

  writer_fputc(ship_info, writer);
  writer_fputc(defences->ship_prob, writer);
  DEBUGF("Finished writing defences data at %ld\n", writer_ftell(writer));
}


void defences_set_timer(DefencesData *const defences, int const timer)
{
  assert(defences);
  assert(timer >= 0);
  defences->timer = timer;
}

int defences_get_timer(DefencesData const *const defences)
{
  assert(defences);
  return defences->timer;
}

void defences_set_ship_type(DefencesData *const defences,
  ShipType const ship_type)
{
  assert(defences);
  assert(ship_type >= ShipType_Fighter1);
  assert(ship_type <= ShipType_Fighter4);
  defences->ship_type = ship_type;
}

ShipType defences_get_ship_type(DefencesData const *const defences)
{
  assert(defences);
  return defences->ship_type;
}

void defences_set_fire_prob(DefencesData *const defences, int const fire_prob)
{
  assert(defences);
  assert(fire_prob >= DefencesMinProb);
  assert(fire_prob <= DefencesMaxProb);
  defences->fire_prob = fire_prob;
}

int defences_get_fire_prob(DefencesData const *const defences)
{
  assert(defences);
  return defences->fire_prob;
}

void defences_set_laser_type(DefencesData *const defences, int const laser_type)
{
  assert(defences);
  assert(laser_type >= DefencesMinLaserType);
  assert(laser_type <= DefencesMaxLaserType);
  defences->laser_type = laser_type;
}

int defences_get_laser_type(DefencesData const *const defences)
{
  assert(defences);
  return defences->laser_type;
}

void defences_set_ships_per_hangar(DefencesData *const defences, int const ships_per_hangar)
{
  assert(defences);
  assert(ships_per_hangar >= 0);
  assert(ships_per_hangar <= (DefencesShipsPerHangarMask >> DefencesShipsPerHangarShift));
  defences->ships_per_hangar = ships_per_hangar;
}

int defences_get_ships_per_hangar(DefencesData const *const defences)
{
  assert(defences);
  return defences->ships_per_hangar;
}

void defences_set_ship_prob(DefencesData *const defences, int const ship_prob)
{
  assert(defences);
  assert(ship_prob >= DefencesMinProb);
  assert(ship_prob <= DefencesMaxProb);
  defences->ship_prob = ship_prob;
}

int defences_get_ship_prob(DefencesData const *const defences)
{
  assert(defences);
  return defences->ship_prob;
}
