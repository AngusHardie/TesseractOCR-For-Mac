/* -*-C-*-
 ********************************************************************************
 *
 * File:        findseam.h  (Formerly findseam.h)
 * Description:
 * Author:       Mark Seaman, SW Productivity
 * Created:      Fri Oct 16 14:37:00 1987
 * Modified:     Thu May 16 17:05:17 1991 (Mark Seaman) marks@hpgrlt
 * Language:     C
 * Package:      N/A
 * Status:       Reusable Software Component
 *
 * (c) Copyright 1987, Hewlett-Packard Company.
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
 *********************************************************************************/

#ifndef FINDSEAM_H
#define FINDSEAM_H

/*----------------------------------------------------------------------
              I n c l u d e s
----------------------------------------------------------------------*/
#include "seam.h"
#include "oldheap.h"
#include "chop.h"

typedef HEAP *SEAM_QUEUE;
typedef ARRAY SEAM_PILE;
/*----------------------------------------------------------------------
              F u n c t i o n s
----------------------------------------------------------------------*/
void junk_worst_seam(SEAM_QUEUE seams, SEAM *new_seam, float new_priority);

void choose_best_seam(SEAM_QUEUE seam_queue,
                      SEAM_PILE *seam_pile,
                      SPLIT *split,
                      PRIORITY priority,
                      SEAM **seam_result,
                      TBLOB *blob);

void combine_seam(SEAM_QUEUE seam_queue, SEAM_PILE seam_pile, SEAM *seam);

inT16 constrained_split(SPLIT *split, TBLOB *blob);

void delete_seam_pile(SEAM_PILE seam_pile);

SEAM *pick_good_seam(TBLOB *blob);

PRIORITY seam_priority(SEAM *seam, inT16 xmin, inT16 xmax);

void try_point_pairs (EDGEPT * points[MAX_NUM_POINTS],
inT16 num_points,
SEAM_QUEUE seam_queue,
SEAM_PILE * seam_pile, SEAM ** seam, TBLOB * blob);

void try_vertical_splits (EDGEPT * points[MAX_NUM_POINTS],
inT16 num_points,
SEAM_QUEUE seam_queue,
SEAM_PILE * seam_pile, SEAM ** seam, TBLOB * blob);
#endif
