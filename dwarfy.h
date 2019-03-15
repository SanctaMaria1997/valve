/*

BSD 2-Clause License

Copyright (c) 2019, SanctaMaria1997
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef DWARFY_H
#define DWARFY_H

#ifdef LINUX
#include "queue.h"
#include "tree.h"
#elif defined(FREEBSD)
#include <sys/queue.h>
#include <sys/tree.h>
#endif

typedef LIST_HEAD(DwarfySourceRecordList,DwarfySourceRecord) DwarfySourceRecordList_t;
typedef LIST_HEAD(DWARF_DATAList,DWARF_DATA) DWARF_DATAList_t;
typedef LIST_HEAD(DwarfyCompilationUnitList,DwarfyCompilationUnit) DwarfyCompilationUnitList_t;
typedef LIST_HEAD(DwarfyDIEList,DwarfyDIE) DwarfyDIEList_t;
typedef LIST_HEAD(DwarfyAttributeSpecList,DwarfyAttributeSpec) DwarfyAttributeSpecList_t;
typedef RB_HEAD(DwarfyObjectRecordTree,DwarfyObjectRecord) DwarfyObjectRecordTree_t;
typedef RB_HEAD(DwarfyFunctionTree,DwarfyFunction) DwarfyFunctionTree_t;
typedef RB_HEAD(DwarfySourceCodeTree,DwarfySourceCode) DwarfySourceCodeTree_t;
typedef RB_HEAD(DwarfyAbbreviationTree,DwarfyAbbreviation) DwarfyAbbreviationTree_t;

typedef struct DwarfyDIE DwarfyDIE;

struct DwarfyDIE
{
  unsigned long int abbreviation_code;
  DwarfyDIEList_t children;
  LIST_ENTRY(DwarfyDIE) linkage;
};

typedef struct DwarfyAttributeSpec DwarfyAttributeSpec;

struct DwarfyAttributeSpec
{
  unsigned long int name;
  unsigned long int form;
  LIST_ENTRY(DwarfyAttributeSpec) linkage;
};

typedef struct DwarfyAbbreviation DwarfyAbbreviation;

struct DwarfyAbbreviation
{
  unsigned long int code;
  unsigned long int tag;
  int num_items;
  int has_children;
  DwarfyAttributeSpecList_t specs;
  RB_ENTRY(DwarfyAbbreviation) DwarfyAbbreviationLinks;
};

int dwarfy_compare_abbreviations(DwarfyAbbreviation *a1,DwarfyAbbreviation *a2);

typedef struct DwarfyObjectRecord DwarfyObjectRecord;

struct DwarfyObjectRecord /* maps ELF executable addresses to human-readable locations in source code */ 
{
  unsigned long int address;
  DwarfySourceRecordList_t source_records;
  RB_ENTRY(DwarfyObjectRecord) DwarfyObjectRecordLinks;
};

int dwarfy_compare_object_records_by_address(DwarfyObjectRecord *dlr1,DwarfyObjectRecord *dlr2);

typedef struct DwarfySourceRecord DwarfySourceRecord;

struct DwarfySourceRecord
{
  unsigned int file;
  unsigned int line_number;
  unsigned int column;
  LIST_ENTRY(DwarfySourceRecord) linkage;
};
  
typedef struct DwarfyFunction DwarfyFunction;

struct DwarfyFunction
{
  char *name;
  unsigned long int address;
  RB_ENTRY(DwarfyFunction) DwarfyFunctionLinks;
};

int dwarfy_compare_functions(DwarfyFunction *df1,DwarfyFunction *df2); 

typedef struct DwarfySourceCode DwarfySourceCode;

struct DwarfySourceCode
{
  char *file_name;
  char **source_code;
  int num_lines;
  RB_ENTRY(DwarfySourceCode) DwarfySourceCodeLinks;
};

int dwarfy_compare_source_code(DwarfySourceCode *dsc1,DwarfySourceCode *dsc2); /* actually compares file names */

typedef struct DwarfyCompilationUnit DwarfyCompilationUnit;

struct DwarfyCompilationUnit
{
  DwarfyObjectRecordTree_t line_numbers;
  DwarfyObjectRecordTree_t addresses;
  DwarfyFunctionTree_t functions;
  DwarfyAbbreviationTree_t abbreviations;
  DwarfyDIEList_t DIE_list;
  DwarfySourceCodeTree_t source_code;
  char *include_paths[256];
  char *file_names[256];
  int num_include_paths;
  int num_file_names;
  int directory_indices[256];
  LIST_ENTRY(DwarfyCompilationUnit) linkage;
};

typedef struct DWARF_DATA DWARF_DATA;

struct DWARF_DATA
{
  DwarfyCompilationUnitList_t compilation_units;
  LIST_ENTRY(DWARF_DATA) linkage;
};

typedef struct
{
  unsigned long int address;
  unsigned int file;
  unsigned int line;
  unsigned int column;
  char is_stmt;
  char basic_block;
  char end_sequence;
} DwarfyLineNumberStateMachine;

typedef struct __attribute__((packed)) 
{
  unsigned int unit_length;
  unsigned short version;
  unsigned int abbreviations_offset;
  unsigned char address_size;
} DwarfyCompilationUnitHeader;

typedef struct __attribute__((packed)) 
{
  unsigned int unit_length;
  unsigned short version;
  unsigned int header_length;
  unsigned char minimum_instruction_length;
  //unsigned char maximum_operations_per_instruction;
  unsigned char default_is_stmt;
  signed char line_base;
  unsigned char line_range;
  unsigned char opcode_base;
} DwarfyLineNumberHeader;

#define DWARFY_ARCHITECTURE_ADDRESS_SIZE 8
#define DWARFY_FORMAT_64 1
#define DWARFY_FORMAT_32 2

RB_PROTOTYPE(DwarfyObjectRecordTree,DwarfyObjectRecord,DwarfyObjectRecordLinks,dwarfy_compare_location_records_by_address)
RB_PROTOTYPE(DwarfyFunctionTree,DwarfyFunction,DwarfyFunctionLinks,dwarfy_compare_functions);
RB_PROTOTYPE(DwarfyAbbreviationTree,DwarfyAbbreviation,DwarfyAbbreviationLinks,dwarfy_compare_location_records_by_address)
RB_PROTOTYPE(DwarfySourceCodeTree,DwarfySourceCode,DwarfySourceCodeLinks,dwarfy_compare_source_code);

DWARF_DATA *load_dwarf(char *file_name,unsigned long int runtime_address);
DWARF_DATA *dwarfy_load_debug_info(unsigned char *elf);
DWARF_DATA *dwarfy_main(void);
void dwarfy_consume_compilation_units(DwarfyCompilationUnitList_t *compilation_units,unsigned char **address);
void dwarfy_consume_DIEs(DwarfyDIEList_t *DIE_list,DwarfyCompilationUnit *compilation_unit,unsigned char **address);
void dwarfy_consume_abbreviations(DwarfyCompilationUnit *compilation_unit,unsigned char **address);
DwarfyAbbreviation *dwarfy_consume_abbreviation_header(unsigned char **address);
void dwarfy_consume_abbreviation_attribute_specs(DwarfyAbbreviation *abbreviation,unsigned char **address);
void dwarfy_consume_line_numbers(DwarfyCompilationUnit *compilation_unit,unsigned char **address);
DwarfyLineNumberHeader *dwarfy_consume_line_number_header(DwarfyCompilationUnit *compilation_unit,unsigned char **address);
void dwarfy_init_line_number_state_machine(DwarfyLineNumberStateMachine *state_machine,DwarfyLineNumberHeader* line_number_header);
void dwarfy_execute_line_number_program(DwarfyCompilationUnit *compilation_unit,DwarfyLineNumberHeader *line_number_header,unsigned char **address);
void dwarfy_line_number_state_machine_out(DwarfyLineNumberStateMachine *state_machine,DwarfyCompilationUnit *compilation_unit);
void dwarfy_load_source_code(DwarfyCompilationUnit *compilation_unit);
long int dwarfy_consume_signed_LEB128(unsigned char **address);
unsigned long int dwarfy_consume_unsigned_LEB128(unsigned char **address);
char *dwarfy_tag_to_string(unsigned long int tag);
char *dwarfy_attribute_to_string(unsigned long int attribute);
char *dwarfy_form_to_string(unsigned long int form);

#endif
