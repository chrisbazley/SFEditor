/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Game file data type definitions
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef DataType_h
#define DataType_h

typedef enum {
  DataType_First,

  /* Data types referenced by file name in a mission file.
     Do not reorder these definitions! */
  DataType_BaseMap = DataType_First,
  DataType_OverlayMap,
  DataType_BaseObjects,
  DataType_OverlayObjects,
  DataType_OverlayMapAnimations,
  DataType_MapTextures,
  DataType_PolygonMeshes,
  DataType_SkyColours,
  DataType_SkyImages,
  DataType_PolygonColours,
  DataType_HillColours,
  DataType_FilenamesCount,

  /* Other editable data types */
  DataType_BaseMapAnimations = DataType_FilenamesCount,
  DataType_Mission,
  DataType_SessionCount,

  /* Data types that aren't directly editable but need
     mapping to platform-specific file types */
  DataType_MapTransfer = DataType_SessionCount,
  DataType_ObjectsTransfer,
  DataType_InfosTransfer,
  DataType_Count
}
DataType;

#endif
