/*
 *  SFeditor - Star Fighter 3000 map/mission editor
 *  Text string data
 *  Copyright (C) 2020 Christopher Bazley
 */

#ifndef TextData_h
#define TextData_h

#include "Text.h"
#include "StringBuff.h"

struct Text
{
  TextParams params;
  StringBuffer string;
};

#endif
