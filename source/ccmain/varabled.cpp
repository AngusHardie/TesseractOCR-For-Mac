///////////////////////////////////////////////////////////////////////
// File:        varabled.cpp
// Description: Variables Editor
// Author:      Joern Wanke
// Created:     Wed Jul 18 10:05:01 PDT 2007
//
// (C) Copyright 2007, Google Inc.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////
//
// The variables editor is used to edit all the variables used within
// tesseract from the ui.
#ifdef WIN32
#else
#include <stdlib.h>
#include <stdio.h>
#endif

#include <map>

// Include automatically generated configuration file if running autoconf.
#ifdef HAVE_CONFIG_H
#include "config_auto.h"
#endif

#ifndef GRAPHICS_DISABLED
#include "varabled.h"


#include "scrollview.h"
#include "svmnode.h"

#include "varable.h"
#include "mainblk.h"

#define VARDIR        "configs/" /*variables files */
#define MAX_ITEMS_IN_SUBMENU 30

const ERRCODE NO_VARIABLES_TO_EDIT = "No Variables defined to edit";

// Contains the mappings from unique VC ids to their actual pointers.
static std::map<int, VariableContent*> vcMap;

static int nrVariables = 0;
static int writeCommands[2];

ELISTIZE(VariableContent)

// Constructors for the various VarTypes.
VariableContent::VariableContent(STRING_VARIABLE* it) {
  my_id_ = nrVariables;
  nrVariables++;
  var_type_ = VT_STRING;
  sIt = it;
  vcMap[my_id_] = this;
}
// Constructors for the various VarTypes.
VariableContent::VariableContent(INT_VARIABLE* it) {
  my_id_ = nrVariables;
  nrVariables++;
  var_type_ = VT_INTEGER;
  iIt = it;
  vcMap[my_id_] = this;
}
// Constructors for the various VarTypes.
VariableContent::VariableContent(BOOL_VARIABLE* it) {
  my_id_ = nrVariables;
  nrVariables++;
  var_type_ = VT_BOOLEAN;
  bIt = it;
  vcMap[my_id_] = this;
}
// Constructors for the various VarTypes.
VariableContent::VariableContent(double_VARIABLE* it) {
  my_id_ = nrVariables;
  nrVariables++;
  var_type_ = VT_DOUBLE;
  dIt = it;
  vcMap[my_id_] = this;
}

// Gets a VC object identified by its ID.
VariableContent* VariableContent::GetVariableContentById(int id) {
  return vcMap[id];
}

// Copy the first N words from the source string to the target string.
// Words are delimited by "_".
void VariablesEditor::GetFirstWords(
                     const char *s,  // source string
                     int n,          // number of words
                     char *t         // target string
                    ) {
  int full_length = strlen(s);
  int reqd_len = 0;              // No. of chars requird
  const char *next_word = s;

  while ((n > 0) && reqd_len < full_length) {
    reqd_len += strcspn(next_word, "_") + 1;
    next_word += reqd_len;
    n--;
  }
  strncpy(t, s, reqd_len);
  t[reqd_len] = '\0';            // ensure null terminal
}

// Getter for the name.
const char* VariableContent::GetName() const {
  if (var_type_ == VT_INTEGER) { return iIt->name_str(); }
  else if (var_type_ == VT_BOOLEAN) { return bIt->name_str(); }
  else if (var_type_ == VT_DOUBLE) { return dIt->name_str(); }
  else if (var_type_ == VT_STRING) { return sIt->name_str(); }
  else
    return "ERROR: VariableContent::GetName()";
}

// Getter for the description.
const char* VariableContent::GetDescription() const {
  if (var_type_ == VT_INTEGER) { return iIt->info_str(); }
  else if (var_type_ == VT_BOOLEAN) { return bIt->info_str(); }
  else if (var_type_ == VT_DOUBLE) { return dIt->info_str(); }
  else if (var_type_ == VT_STRING) { return sIt->info_str(); }
  else return NULL;
}

// Getter for the value.
const char* VariableContent::GetValue() const {
char* msg = new char[1024];
  if (var_type_ == VT_INTEGER) {
    sprintf(msg, "%d", ((inT32) *(iIt)));
  } else if (var_type_ == VT_BOOLEAN) {
    sprintf(msg, "%d", ((BOOL8) * (bIt)));
  } else if (var_type_ == VT_DOUBLE) {
    sprintf(msg, "%g", ((double) * (dIt)));
  } else if (var_type_ == VT_STRING) {
    if (((STRING) * (sIt)).string() != NULL) {
      sprintf(msg, "%s", ((STRING) * (sIt)).string());
    } else {
      sprintf(msg, "Null");
    }
  }
  return msg;
}

// Setter for the value.
void VariableContent::SetValue(const char* val) {
// TODO (wanke) Test if the values actually are properly converted.
// (Quickly visible impacts?)
  changed_ = TRUE;
  if (var_type_ == VT_INTEGER) {
    iIt->set_value(atoi(val));
  } else if (var_type_ == VT_BOOLEAN) {
    bIt->set_value(atoi(val));
  } else if (var_type_ == VT_DOUBLE) {
    dIt->set_value(strtod(val, NULL));
  } else if (var_type_ == VT_STRING) {
    sIt->set_value(val);
  }
}

// Gets the up to the first 3 prefixes from s (split by _).
// For example, tesseract_foo_bar will be split into tesseract,foo and bar.
void VariablesEditor::GetPrefixes(const char* s, STRING* level_one,
                                                 STRING* level_two,
                                                 STRING* level_three) {
  char* p = new char[1024];
  GetFirstWords(s, 1, p);
  *level_one = p;
  GetFirstWords(s, 2, p);
  *level_two = p;
  GetFirstWords(s, 3, p);
  *level_three = p;
  delete[] p;
}

// Compare two VC objects by their name.
int VariableContent::Compare(const void* v1, const void* v2) {
  const VariableContent* one =
    *reinterpret_cast<const VariableContent* const *>(v1);
  const VariableContent* two =
    *reinterpret_cast<const VariableContent* const *>(v2);
  return strcmp(one->GetName(), two->GetName());
}

// Find all editable variables used within tesseract and create a
// SVMenuNode tree from it.
// TODO (wanke): This is actually sort of hackish.
SVMenuNode* VariablesEditor::BuildListOfAllLeaves() {  // find all variables.
  SVMenuNode* mr = new SVMenuNode();
  VariableContent_LIST vclist;
  VariableContent_IT vc_it(&vclist);
  // Amount counts the number of entries for a specific char*.
  // TODO(rays) get rid of the use of std::map.
  std::map<const char*, int> amount;

  INT_VARIABLE_C_IT int_it(INT_VARIABLE::get_head());
  BOOL_VARIABLE_C_IT bool_it(BOOL_VARIABLE::get_head());
  STRING_VARIABLE_C_IT str_it(STRING_VARIABLE::get_head());
  double_VARIABLE_C_IT dbl_it(double_VARIABLE::get_head());

  // Add all variables to a list.
  for (int_it.mark_cycle_pt(); !int_it.cycled_list(); int_it.forward()) {
    vc_it.add_after_then_move(new VariableContent(int_it.data()));
  }

  for (bool_it.mark_cycle_pt(); !bool_it.cycled_list(); bool_it.forward()) {
    vc_it.add_after_then_move(new VariableContent(bool_it.data()));
  }

  for (str_it.mark_cycle_pt(); !str_it.cycled_list(); str_it.forward()) {
    vc_it.add_after_then_move(new VariableContent(str_it.data()));
  }

  for (dbl_it.mark_cycle_pt(); !dbl_it.cycled_list(); dbl_it.forward()) {
    vc_it.add_after_then_move(new VariableContent(dbl_it.data()));
  }

  // Count the # of entries starting with a specific prefix.
  for (vc_it.mark_cycle_pt(); !vc_it.cycled_list(); vc_it.forward()) {
    VariableContent* vc = vc_it.data();
    STRING tag;
    STRING tag2;
    STRING tag3;

    GetPrefixes(vc->GetName(), &tag, &tag2, &tag3);
    amount[tag.string()]++;
    amount[tag2.string()]++;
    amount[tag3.string()]++;
  }

  vclist.sort(VariableContent::Compare);  // Sort the list alphabetically.

  SVMenuNode* other = mr->AddChild("OTHER");

  // go through the list again and this time create the menu structure.
  vc_it.move_to_first();
  for (vc_it.mark_cycle_pt(); !vc_it.cycled_list(); vc_it.forward()) {
    VariableContent* vc = vc_it.data();
    STRING tag;
    STRING tag2;
    STRING tag3;
    GetPrefixes(vc->GetName(), &tag, &tag2, &tag3);

    if (amount[tag.string()] == 1) { other->AddChild(vc->GetName(), vc->GetId(),
                                            vc->GetValue(),
                                            vc->GetDescription());
    } else {  // More than one would use this submenu -> create submenu.
      SVMenuNode* sv = mr->AddChild(tag.string());
      if ((amount[tag.string()] <= MAX_ITEMS_IN_SUBMENU) ||
          (amount[tag2.string()] <= 1)) {
        sv->AddChild(vc->GetName(), vc->GetId(),
                     vc->GetValue(), vc->GetDescription());
      } else {  // Make subsubmenus.
        SVMenuNode* sv2 = sv->AddChild(tag2.string());
        sv2->AddChild(vc->GetName(), vc->GetId(),
                      vc->GetValue(), vc->GetDescription());
      }
    }
  }
  return mr;
}

// Event listener. Waits for SVET_POPUP events and processes them.
void VariablesEditor::Notify(const SVEvent* sve) {
  if (sve->type == SVET_POPUP) {  // only catch SVET_POPUP!
    char* param = sve->parameter;
    if (sve->command_id == writeCommands[0]) {
      WriteVars(param, false);
    } else if (sve->command_id == writeCommands[1]) {
      WriteVars(param, true);
    } else {
      VariableContent* vc = VariableContent::GetVariableContentById(
          sve->command_id);
      vc->SetValue(param);
      sv_window_->AddMessage("Setting %s to %s",
                             vc->GetName(), vc->GetValue());
    }
  }
}

// Integrate the variables editor as popupmenu into the existing scrollview
// window (usually the pg editor). If sv == null, create a new empty
// empty window and attach the variables editor to that window (ugly).
VariablesEditor::VariablesEditor(const tesseract::Tesseract* tess,
                                 ScrollView* sv) {
  if (sv == NULL) {
    const char* name = "VarEditorMAIN";
    sv = new ScrollView(name, 1, 1, 200, 200, 300, 200);
  }

  sv_window_ = sv;

  //Only one event handler per window.
  //sv->AddEventHandler((SVEventHandler*) this);

  SVMenuNode* svMenuRoot = BuildListOfAllLeaves();

  STRING varfile;
  varfile = tess->datadir;
  varfile += VARDIR;             // variables dir
  varfile += "edited";           // actual name

  SVMenuNode* std_menu = svMenuRoot->AddChild ("Build Config File");

  writeCommands[0] = nrVariables+1;
  std_menu->AddChild("All Variables", writeCommands[0],
                     varfile.string(), "Config file name?");

  writeCommands[1] = nrVariables+2;
  std_menu->AddChild ("changed_ Variables Only", writeCommands[1],
                      varfile.string(), "Config file name?");

  svMenuRoot->BuildMenu(sv, false);
}


// Write all (changed_) variables to a config file.
void VariablesEditor::WriteVars(char *filename,    // in this file
                                bool changes_only  // changed_ vars only?
                               ) {
  FILE *fp;                      // input file
  char msg_str[255];
                                 // if file exists
  if ((fp = fopen (filename, "r")) != NULL) {
    fclose(fp);
    sprintf (msg_str, "Overwrite file " "%s" "? (Y/N)", filename);
    int a = sv_window_->ShowYesNoDialog(msg_str);
    if (a == 'n') { return; }  // dont write
  }


  fp = fopen (filename, "w");  // can we write to it?
  if (fp == NULL) {
    sv_window_->AddMessage("Cant write to file " "%s" "", filename);
    return;
  }

  for (std::map<int, VariableContent*>::iterator iter = vcMap.begin();
                                          iter != vcMap.end();
                                          ++iter) {
    VariableContent* cur = iter->second;
    if (!changes_only || cur->HasChanged()) {
      fprintf (fp, "%-25s   %-12s   # %s\n",
               cur->GetName(), cur->GetValue(), cur->GetDescription());
    }
  }
  fclose(fp);
}
#endif
