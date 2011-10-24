/* -*-C-*-
 ********************************************************************************
 * File:        hyphen.c  (Formerly hyphen.c)
 * Description: Functions for maintaining information about hyphenated words.
 * Author:       Mark Seaman, OCR Technology
 * Created:      Fri Oct 16 14:37:00 1987
 * Modified:     Thu Mar 14 11:09:43 1991 (Mark Seaman) marks@hpgrlt
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

#include "dict.h"

INT_VAR(hyphen_debug_level, 0, "Debug level for hyphenated words.");

namespace tesseract {

// Unless the previous word was the last one on the line, and the current
// one is not (thus it is the first one on the line), erase hyphen_word_,
// clear hyphen_active_dawgs_, hyphen_constraints_ update last_word_on_line_.
void Dict::reset_hyphen_vars(bool last_word_on_line) {
  if (!(last_word_on_line_ == true && last_word_on_line == false)) {
    if (hyphen_word_ != NULL) {
      delete hyphen_word_;
      hyphen_word_ = NULL;
      hyphen_active_dawgs_.clear();
      hyphen_constraints_.clear();
    }
  }
  if (hyphen_debug_level) {
    tprintf("reset_hyphen_vars: last_word_on_line %d -> %d\n",
            last_word_on_line_, last_word_on_line);
  }
  last_word_on_line_ = last_word_on_line;
}

// Update hyphen_word_, and copy the given DawgInfoVectors into
// hyphen_active_dawgs_ and hyphen_constraints_.
void Dict::set_hyphen_word(const WERD_CHOICE &word,
                           const DawgInfoVector &active_dawgs,
                           const DawgInfoVector &constraints) {
  if (hyphen_word_ == NULL) {
    hyphen_word_ = new WERD_CHOICE();
    hyphen_word_->make_bad();
  }
  if (hyphen_word_->rating() > word.rating()) {
    *hyphen_word_ = word;
    hyphen_word_->remove_last_unichar_id();  // last unichar id is a hyphen
    hyphen_active_dawgs_ = active_dawgs;
    hyphen_constraints_ = constraints;
  }
  if (hyphen_debug_level) {
    hyphen_word_->print("set_hyphen_word: ");
  }
}
}  // namespace tesseract
