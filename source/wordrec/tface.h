/**********************************************************************
 * File:        tface.h
 * Description: C side of the Tess/tessedit C/C++ interface.
 * Author:		Ray Smith
 * Created:		Mon Apr 27 11:57:06 BST 1992
 *
 * (C) Copyright 1992, Hewlett-Packard Ltd.
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 ** http://www.apache.org/licenses/LICENSE-2.0
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 *
 **********************************************************************/

#ifndef TFACE_H
#define TFACE_H

#include "cutil.h"
#include "host.h"
#include "ratngs.h"
#include "tessclas.h"

extern BOOL_VAR_H(wordrec_no_block, false, "Don't output block information");

/*----------------------------------------------------------------------------
          Function Prototypes
----------------------------------------------------------------------------*/
void edit_with_ocr(const char *imagename);

#endif
