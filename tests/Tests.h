/* SFEditor test: test suite definitions
 * Copyright (C) 2026 Christopher Bazley
 */
#ifndef Tests_h
#define Tests_h

void MapEditSel_tests(void);
void ObjEditSel_tests(void);
void LoadGameData_tests(void);

#ifdef FORTIFY
#include "fortify.h"
#else
#define Fortify_EnterScope()
#define Fortify_LeaveScope()
#define Fortify_OutputStatistics()
#endif

#endif /* Tests_h */
