/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Mission ships
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
#include <limits.h>
#include "stdlib.h"

#include "Debug.h"
#include "Macros.h"
#include "Reader.h"
#include "Writer.h"
#include "LinkedList.h"

#include "SFError.h"
#include "CoarseCoord.h"
#include "Ships.h"
#include "ShipsData.h"
#include "Paths.h"

enum {
  BytesPerShip = 32,
  ShipFollowPlayer = 255,
  ShipPilotMask = 0xf,
  ShipPilotShift = 0,
  ShipDirMask = 0xf0,
  ShipDirShift = 4,

  /* Constants for first flags byte in SFShip structure */
  ShipFlag_ModeMask = 0x3,     /* see definition of SFShipMode */
  ShipFlag_Friendly = 1u << 2, /* otherwise enemy */
  ShipFlag_Cloaked  = 1u << 3,

  /* Constants for second flags byte in SFShip structure */
  ShipFlag2_IgnoreAttacks   = 1u << 0,
  ShipFlag2_ProximityAction = 1u << 1, /* join formation or attack */
};

typedef enum
{
  ShipMode_Attack,
  ShipMode_FlightPath,
  ShipMode_FollowShip
}
ShipMode;

struct Ship
{
  ShipsData *ships;
  LinkedListItem link;
  FinePoint3d coords;
  ShipDirection direction;
  ShipType type;
  ShipBehaviour behaviour;
  ShipMission importance;
  ShipMode mode;
  ShipFlags flags;
  ShipPilot pilot;
  int index; /* valid for ShipsDataState_Write */
  union {
    union {
      int num; /* valid for ShipsDataState_PostRead */
      Ship *ship; /* null means attack player */
    } attack;
    struct {
      int path_num; /* valid for ShipsDataState_PostRead */
      union {
        int num; /* valid for ShipsDataState_PostRead */
        Waypoint *waypoint;
      } start;
    } flightpath;
    struct {
      union {
        int num; /* valid for ShipsDataState_PostRead */
        Ship *ship; /* null means follow player */
      } leader;
      FinePoint3d offset;
    } follow;
  } mode_data; /* valid for ShipsDataState_PreWrite and ShipsDataState_Write */
  LinkedList ref_list;
  LinkedListItem ref_link;
};

void ships_init(ShipsData *const ships)
{
  assert(ships);
  linkedlist_init(&ships->list);
  *ships = (ShipsData){.count = 0, .state = ShipsDataState_PreWrite};
}

void ships_destroy(ShipsData *const ships)
{
  assert(ships);

  LINKEDLIST_FOR_EACH_SAFE(&ships->list, item, tmp)
  {
    Ship *const ship = CONTAINER_OF(item, Ship, link);
    free(ship);
  }
}

SFError ships_add(ShipsData *const ships,
  FinePoint3d coords,
  ShipDirection direction,
  ShipType type,
  ShipBehaviour behaviour,
  ShipMission importance,
  ShipFlags flags,
  ShipPilot pilot,
  Ship **const new_ship)
{
  assert(ships);
  assert(ships->state != ShipsDataState_PostRead);
  assert(ships->count >= 0);
  assert(ships->count <= ShipsMax);

  if (ships->count == ShipsMax)
  {
    return SFERROR(NumShips);
  }

  Ship *const ship = malloc(sizeof(*ship));
  if (!ship)
  {
    return SFERROR(NoMem);
  }

  *ship = (Ship){
    .ships = ships,
    .coords = coords,
    .direction = direction,
    .type = type,
    .behaviour = behaviour,
    .importance = importance,
    .mode = ShipMode_Attack,
    .flags = flags,
    .pilot = pilot,
  };
  linkedlist_init(&ship->ref_list);

  linkedlist_insert(&ships->list,
    linkedlist_get_tail(&ships->list),
    &ship->link);

  ships->count++;
  ships->state = ShipsDataState_PreWrite;

  if (new_ship) {
    *new_ship = ship;
  }
  return SFERROR(OK);
}

void ship_set_flightpath(Ship *const ship, Waypoint *const waypoint)
{
  assert(ship);
  assert(ship->ships);
  assert(ship->ships->state != ShipsDataState_PostRead);
  assert(waypoint);

  ship->mode = ShipMode_FlightPath;
  ship->mode_data.flightpath.start.waypoint = waypoint;
}

void ship_set_follow(Ship *const ship, Ship *const leader,
  FinePoint3d const offset)
{
  assert(ship);
  assert(ship->ships);
  assert(ship->ships->state != ShipsDataState_PostRead);

  ship->mode = ShipMode_FollowShip;
  ship->mode_data.follow.leader.ship = leader; /* null means follow player */
  ship->mode_data.follow.offset = offset;
}

void ship_set_attack(Ship *const ship, Ship *const target)
{
  assert(ship);
  assert(ship->ships);
  assert(ship->ships->state != ShipsDataState_PostRead);

  ship->mode = ShipMode_Attack;
  ship->mode_data.attack.ship = target; /* null means attack player */
}

void ship_delete(Ship *const ship)
{
  assert(ship);
  assert(ship->ships);
  assert(ship->ships->state != ShipsDataState_PostRead);

  ShipsData *const ships = ship->ships;
  linkedlist_remove(&ships->list, &ship->link);
  assert(ships->count > 0);
  --ships->count;
  ships->state = ShipsDataState_PreWrite;

  free(ship);
}

SFError ships_read_pad(ShipsData *const ships, Reader *const reader)
{
  SFError err = ships_read(ships, reader);
  if (SFError_fail(err)) {
    return err;
  }

  assert(ShipsMax <= LONG_MAX);
  long int const padding = ShipsMax - (long)ships->count;
  if (reader_fseek(reader, padding * BytesPerShip, SEEK_CUR)) {
    return SFERROR(BadSeek);
  }
  DEBUGF("Finished reading ships data at %ld\n", reader_ftell(reader));
  return SFERROR(OK);
}

SFError ships_read(ShipsData *const ships, Reader *const reader)
{
  assert(ships);

  int32_t num_ships = 0;
  if (!reader_fread_int32(&num_ships, reader))
  {
    return SFERROR(ReadFail);
  }

  if (num_ships < 0 || num_ships > ShipsMax)
  {
    return SFERROR(BadNumShips);
  }

  for (int32_t j = 0; j < num_ships; ++j)
  {
    DEBUGF("Reading ship %" PRId32 " data at %ld\n", j, reader_ftell(reader));
    FinePoint3d coords = {0};
    if (!FinePoint3d_read(&coords, reader))
    {
      return SFERROR(ReadFail);
    }

    int const type = reader_fgetc(reader);
    if (type == EOF) {
      return SFERROR(ReadFail);
    }
    if (type < ShipType_Fighter1 ||
        (type > ShipType_Fighter4 && type < ShipType_Big1) ||
        (type > ShipType_Big3 && type != ShipType_Satellite)) {
      return SFERROR(BadShipType);
    }

    int const flags = reader_fgetc(reader);
    if (flags == EOF) {
      return SFERROR(ReadFail);
    }

    int const mode = flags & ShipFlag_ModeMask;
    if (mode < ShipMode_Attack || mode > ShipMode_FollowShip)
    {
      return SFERROR(BadShipMode);
    }

    if (flags & ~(ShipFlag_ModeMask|ShipFlag_Friendly|ShipFlag_Cloaked))
    {
      return SFERROR(ReservedShipBits);
    }

    int const waypoint_num = reader_fgetc(reader);
    if (waypoint_num == EOF) {
      return SFERROR(ReadFail);
    }

    int const ship_or_path_num = reader_fgetc(reader);
    if (ship_or_path_num == EOF) {
      return SFERROR(ReadFail);
    }

    FinePoint3d follow_offset = {0};
    if (!FinePoint3d_read(&follow_offset, reader))
    {
      return SFERROR(ReadFail);
    }

    int const importance = reader_fgetc(reader);
    if (importance == EOF) {
      return SFERROR(ReadFail);
    }

    if (importance < ShipMission_NotImportant ||
        importance > ShipMission_PreventLanding) {
      return SFERROR(BadShipGoal);
    }

    int const flags2 = reader_fgetc(reader);
    if (flags2 == EOF) {
      return SFERROR(ReadFail);
    }

    ShipFlags const sflags = {
      .is_friendly = flags & ShipFlag_Friendly,
      .is_cloaked = flags & ShipFlag_Cloaked,
      .is_pacifist = flags2 & ShipFlag2_IgnoreAttacks,
      .is_alert = flags2 & ShipFlag2_ProximityAction};

    if (flags2 & ~(ShipFlag2_IgnoreAttacks|ShipFlag2_ProximityAction)) {
      return SFERROR(ReservedShipBits2);
    }

    int const behaviour = reader_fgetc(reader);
    if (behaviour == EOF) {
      return SFERROR(ReadFail);
    }

    if (behaviour < ShipBehaviour_Moving ||
        behaviour > ShipBehaviour_Stationary) {
      return SFERROR(BadShipMotion);
    }

    int const dir_and_pilot = reader_fgetc(reader);
    if (dir_and_pilot == EOF) {
      return SFERROR(ReadFail);
    }

    ShipPilot const pilot = (dir_and_pilot & ShipPilotMask) >> ShipPilotShift;
    int const direction = (dir_and_pilot & ShipDirMask) >> ShipDirShift;

    if (direction < ShipDirection_S || direction > ShipDirection_SW) {
      return SFERROR(BadShipDir);
    }

    Ship *ship = NULL;
    SFError err = ships_add(ships, coords, direction, type,
                            behaviour, importance, sflags, pilot, &ship);
    if (SFError_fail(err)) {
      return err;
    }

    ship->mode = mode;
    switch (mode)
    {
    case ShipMode_FlightPath:
      DEBUGF("Ship %" PRId32 " is on flight path %" PRId32
             " starting at waypoint %" PRId32 "\n",
             j, ship_or_path_num, waypoint_num);
      ship->mode_data.flightpath.start.num = waypoint_num;
      ship->mode_data.flightpath.path_num = ship_or_path_num;
      break;
    case ShipMode_FollowShip:
      DEBUGF("Ship %" PRId32 " is following ship %" PRId32 "\n",
             j, ship_or_path_num);
      ship->mode_data.follow.offset = follow_offset;
      ship->mode_data.follow.leader.num = ship_or_path_num;
      break;
    case ShipMode_Attack:
      DEBUGF("Ship %" PRId32 " is attacking ship %" PRId32 "\n",
             j, ship_or_path_num);
      ship->mode_data.attack.num = ship_or_path_num;
      break;
    default:
      break;
    }
  }

  ships->state = ShipsDataState_PostRead;
  return SFERROR(OK);
}

static inline SFError post_read_ship_flightpath(Ship *const ship,
  PathsData *const paths)
{
  assert(ship);

  Path *const path = path_from_index(paths, ship->mode_data.flightpath.path_num);
  if (!path)
  {
    return SFERROR(BadShipPath);
  }

  ship->mode_data.flightpath.start.waypoint = waypoint_from_index(
    path, ship->mode_data.flightpath.start.num);

  if (!ship->mode_data.flightpath.start.waypoint)
  {
    return SFERROR(BadShipWaypoint);
  }

  return SFERROR(OK);
}

static inline SFError post_read_ship_attack(Ship *const ship)
{
  assert(ship);

  if (ship->mode_data.attack.num == ShipFollowPlayer)
  {
    ship->mode_data.attack.ship = NULL;
  }
  else
  {
    ship->mode_data.attack.ship =
      ship_from_index(ship->ships, ship->mode_data.attack.num);

    if (!ship->mode_data.attack.ship)
    {
      return SFERROR(BadShipTarget);
    }
  }
  return SFERROR(OK);
}

static inline SFError post_read_ship_follow(Ship *const ship)
{
  assert(ship);

  if (ship->mode_data.follow.leader.num == ShipFollowPlayer)
  {
    ship->mode_data.follow.leader.ship = NULL;
  }
  else
  {
    ship->mode_data.follow.leader.ship =
      ship_from_index(ship->ships, ship->mode_data.follow.leader.num);

    if (!ship->mode_data.follow.leader.ship)
    {
      return SFERROR(BadShipLeader);
    }
  }

  return SFERROR(OK);
}

static inline SFError post_read_ship(Ship *const ship,
  PathsData *const paths)
{
  /* Validation that had to be deferred because of the data order in the
     source file. Also converts numbers to pointers. */
  assert(ship);

  SFError err = SFERROR(OK);
  switch (ship->mode)
  {
  case ShipMode_FlightPath:
    err = post_read_ship_flightpath(ship, paths);
    break;
  case ShipMode_Attack:
    err = post_read_ship_attack(ship);
    break;
  case ShipMode_FollowShip:
    err = post_read_ship_follow(ship);
    break;
  default:
    break;
  }
  return err;
}

SFError ships_post_read(ShipsData *ships, PathsData *paths)
{
  assert(ships);
  assert(ships->state == ShipsDataState_PostRead);
  SFError err = SFERROR(OK);

  LINKEDLIST_FOR_EACH(&ships->list, item)
  {
    err = post_read_ship(CONTAINER_OF(item, Ship, link), paths);
    if (SFError_fail(err))
    {
      break;
    }
  }

  ships->state = ShipsDataState_PreWrite;
  return err;
}

void ships_pre_write(ShipsData *const ships)
{
  assert(ships);
  int index = 0;

  LINKEDLIST_FOR_EACH(&ships->list, item)
  {
    Ship *const ship = CONTAINER_OF(item, Ship, link);
    ship->index = index++;
  }

  ships->state = ShipsDataState_Write;
}

static void write_ship(Ship const *const ship, Writer *const writer)
{
  assert(ship);
  FinePoint3d_write(ship->coords, writer);

  assert((ship->type >= ShipType_Fighter1 && ship->type <= ShipType_Fighter4) ||
         (ship->type >= ShipType_Big1 && ship->type <= ShipType_Big3) ||
         (ship->type == ShipType_Satellite));

  writer_fputc(ship->type, writer);

  assert(ship->mode >= ShipMode_Attack && ship->mode <= ShipMode_FollowShip);

  int flags = ship->mode & ShipFlag_ModeMask;

  if (ship->flags.is_friendly)
  {
    flags |= ShipFlag_Friendly;
  }

  if (ship->flags.is_cloaked)
  {
    flags |= ShipFlag_Cloaked;
  }

  writer_fputc(flags, writer);

  switch(ship->mode)
  {
  case ShipMode_FlightPath:
    writer_fputc(waypoint_get_index(ship->mode_data.flightpath.start.waypoint), writer);
    writer_fputc(path_get_index(waypoint_get_path(ship->mode_data.flightpath.start.waypoint)), writer);
    FinePoint3d_write((FinePoint3d){0,0,0}, writer);
    break;

  case ShipMode_Attack:
    writer_fputc(0, writer);
    if (ship->mode_data.attack.ship)
    {
      writer_fputc(ship_get_index(ship->mode_data.attack.ship), writer);
    }
    else
    {
      writer_fputc(ShipFollowPlayer, writer);
    }
    FinePoint3d_write((FinePoint3d){0,0,0}, writer);
    break;

  case ShipMode_FollowShip:
    writer_fputc(0, writer);
    if (ship->mode_data.follow.leader.ship)
    {
      writer_fputc(ship_get_index(ship->mode_data.follow.leader.ship), writer);
    }
    else
    {
      writer_fputc(ShipFollowPlayer, writer);
    }
    FinePoint3d_write(ship->mode_data.follow.offset, writer);
    break;
  }

  assert(ship->importance >= ShipMission_NotImportant);
  assert(ship->importance <= ShipMission_PreventLanding);
  writer_fputc(ship->importance, writer);

  int flags2 = 0;

  if (ship->flags.is_pacifist)
  {
    flags2 |= ShipFlag2_IgnoreAttacks;
  }

  if (ship->flags.is_alert)
  {
    flags2 |= ShipFlag2_ProximityAction;
  }

  writer_fputc(flags2, writer);

  assert(ship->behaviour >= ShipBehaviour_Moving);
  assert(ship->behaviour <= ShipBehaviour_Stationary);
  writer_fputc(ship->behaviour, writer);

  int dir_and_pilot = (ship->pilot << ShipPilotShift) & ShipPilotMask;

  assert(ship->direction >= ShipDirection_S);
  assert(ship->direction <= ShipDirection_SW);
  dir_and_pilot |= ((int)ship->direction << ShipDirShift) & ShipDirMask;

  writer_fputc(dir_and_pilot, writer);
}

void ships_write_pad(ShipsData *const ships, Writer *const writer)
{
  ships_write(ships, writer);
  if (writer_ferror(writer)) {
    return;
  }

  size_t const padding = ShipsMax - ships->count;
  writer_fseek(writer, (long)padding * BytesPerShip, SEEK_CUR);
  DEBUGF("Finished writing ships data at %ld\n", writer_ftell(writer));
}

void ships_write(ShipsData *const ships, Writer *const writer)
{
  assert(ships);
  assert(ships->state == ShipsDataState_Write);
  assert(ships->count >= 0);
  assert(ships->count <= ShipsMax);
  assert(ships->count <= INT32_MAX);
  writer_fwrite_int32((int32_t)ships->count, writer);

  LINKEDLIST_FOR_EACH(&ships->list, item)
  {
    Ship *const ship = CONTAINER_OF(item, Ship, link);
    write_ship(ship, writer);
    if (writer_ferror(writer))
    {
      return;
    }
  }
}

size_t ships_get_count(ShipsData const *const ships)
{
  return ships->count;
}

Ship *ship_from_index(ShipsData *const ships, int const index)
{
  /* Only expected to be used on mission load, otherwise we should
     substitute an array */
  assert(ships);

  int i = 0;
  LINKEDLIST_FOR_EACH(&ships->list, item)
  {
    if (index == i++)
    {
      Ship *const ship = CONTAINER_OF(item, Ship, link);
      DEBUGF("Decoded ship index %d as %p\n", index, (void *)ship);
      return ship;
    }
  }
  DEBUGF("Failed to decode ship index %d\n", index);
  return NULL;
}

int ship_get_index(Ship const *const ship)
{
  assert(ship);
  assert(ship->ships);
  assert(ship->ships->state == ShipsDataState_Write);
  DEBUGF("Ship index is %d\n", ship->index);
  return ship->index;
}
