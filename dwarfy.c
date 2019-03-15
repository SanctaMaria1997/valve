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

#include <elf.h>
#ifdef LINUX
#include <libdwarf/dwarf.h>
#elif defined(FREEBSD)
#include <dwarf.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "elf_util.h"
#include "dwarfy.h"

#ifdef FREEBSD
#define DW_LNE_set_discriminator 4
#endif

int DWARFY_FORMAT;
unsigned long int DEBUG_INFO_SIZE;
unsigned long int COMPILATION_UNIT_OFFSET;
unsigned long int COMPILATION_UNIT_LENGTH;
unsigned int LINE_NUMBER_PROGRAM_OFFSET;

unsigned char *DEBUG_INFO,*DEBUG_ABBREV,*DEBUG_LINE,*DEBUG_STR,*DEBUG_ARANGES;

unsigned long int DWARFY_ELF_BASE_ADDRESS;
unsigned long int DWARFY_ELF_RUNTIME_ADDRESS;

int dwarfy_compare_object_records_by_address(DwarfyObjectRecord *dlr1,DwarfyObjectRecord *dlr2)
{
  return dlr2->address - dlr1->address;
}

int dwarfy_compare_abbreviations(DwarfyAbbreviation *a1,DwarfyAbbreviation *a2)
{
  return a2->code - a1->code;
}

int dwarfy_compare_functions(DwarfyFunction *df1,DwarfyFunction *df2)
{
  return df2->address - df1->address;//strcmp(df2->name,df1->name);
}

int dwarfy_compare_source_code(DwarfySourceCode *dsc1,DwarfySourceCode *dsc2)
{
  return strcmp(dsc2->file_name,dsc1->file_name);
}

RB_GENERATE(DwarfyObjectRecordTree,DwarfyObjectRecord,DwarfyObjectRecordLinks,dwarfy_compare_object_records_by_address)
RB_GENERATE(DwarfyFunctionTree,DwarfyFunction,DwarfyFunctionLinks,dwarfy_compare_functions)
RB_GENERATE(DwarfyAbbreviationTree,DwarfyAbbreviation,DwarfyAbbreviationLinks,dwarfy_compare_abbreviations)
RB_GENERATE(DwarfySourceCodeTree,DwarfySourceCode,DwarfySourceCodeLinks,dwarfy_compare_source_code);

DWARF_DATA *load_dwarf(char *file_name,unsigned long int runtime_address)
{
  char debug_file_name[256];
  unsigned char *elf;
  unsigned char *elf_standalone_debug;
  DWARF_DATA *debug_info;
  
  elf = 0;
  elf_standalone_debug = 0;
  
  if(0 == (elf = load_elf(file_name)))
    return 0;
  
  DWARFY_ELF_BASE_ADDRESS = get_elf_base_address(elf);
  DWARFY_ELF_RUNTIME_ADDRESS = runtime_address;

  if(0 == (debug_info = dwarfy_load_debug_info(elf)))
  {
    strcpy(debug_file_name,file_name);
    strcat(debug_file_name,".debug");
    
    if(0 == (elf_standalone_debug = load_elf(debug_file_name)))
    {
      debug_info = 0;
      goto cleanup;
    }
    
    debug_info = dwarfy_load_debug_info(elf_standalone_debug);

  }

  cleanup:
  
  if(elf)
    free(elf);
  
  if(elf_standalone_debug)
    free(elf_standalone_debug);
  
  return debug_info;
}

DWARF_DATA *dwarfy_load_debug_info(unsigned char *elf)
{
  int result;
  char *section_name;
  char debug_file_name[256];
  Elf64_Ehdr *elf_header;
  Elf64_Phdr *program_header;
  unsigned short num_program_headers;
  Elf64_Shdr *section_header;
  unsigned int num_sections;
  unsigned int section_header_size;
  unsigned int section_names_index;
  
  DEBUG_INFO = DEBUG_ABBREV = DEBUG_LINE = DEBUG_STR = 0;

  DEBUG_INFO_SIZE = 0;
  COMPILATION_UNIT_OFFSET = 0;
  COMPILATION_UNIT_LENGTH = 0;
  LINE_NUMBER_PROGRAM_OFFSET = 0;

  elf_header = (Elf64_Ehdr*)elf;
  program_header = (Elf64_Phdr*)(elf + elf_header->e_phoff);
  num_program_headers = elf_header->e_phnum;
  section_header = (Elf64_Shdr*)(elf + elf_header->e_shoff);
  num_sections = elf_header->e_shnum;
  section_header_size = elf_header->e_shentsize;
  section_names_index = elf_header->e_shstrndx;
  if(section_names_index == SHN_XINDEX)
    section_names_index = section_header->sh_link;

  int i;
  int m;
  for(i = 0; i < num_sections; i++)
  { 
    section_name = (char*)(elf + section_header[section_names_index].sh_offset + section_header[i].sh_name);
    
    if(!strcmp(section_name,".debug_info"))
    {
      DEBUG_INFO = elf + section_header[i].sh_offset;
      DEBUG_INFO_SIZE = (unsigned long int)section_header[i].sh_size;
    }
    if(!strcmp(section_name,".debug_abbrev"))
      DEBUG_ABBREV = elf + section_header[i].sh_offset;
    if(!strcmp(section_name,".debug_line"))
      DEBUG_LINE = elf + section_header[i].sh_offset;
    if(!strcmp(section_name,".debug_str"))
      DEBUG_STR = elf + section_header[i].sh_offset;
  }
  
  if(0 == (DEBUG_INFO && DEBUG_ABBREV && DEBUG_LINE && DEBUG_STR))
    return 0;
  
  return dwarfy_main();
}

DwarfyCompilationUnit *create_compilation_unit()
{
  DwarfyCompilationUnit *compilation_unit;
  
  compilation_unit = malloc(sizeof(DwarfyCompilationUnit));
  
  RB_INIT(&compilation_unit->line_numbers);
  RB_INIT(&compilation_unit->addresses);
  RB_INIT(&compilation_unit->functions);
  
  compilation_unit->num_include_paths = 1;
  compilation_unit->include_paths[0] = "";
  compilation_unit->num_file_names = 0;
  
  //compilation_unit->abbreviations = malloc(sizeof(DwarfyAbbreviationTree_t));
  RB_INIT(&compilation_unit->abbreviations);
  LIST_INIT(&compilation_unit->DIE_list);
  RB_INIT(&compilation_unit->source_code);
  
  return compilation_unit;
} 
  
DWARF_DATA *dwarfy_main()
{
  DWARF_DATA *elf;
  unsigned char *address;
  
  elf = malloc(sizeof(DWARF_DATA));

  LIST_INIT(&elf->compilation_units);
  dwarfy_consume_compilation_units(&elf->compilation_units,&address);
  return elf;

}

void dwarfy_consume_compilation_units(DwarfyCompilationUnitList_t *compilation_units,unsigned char **address)
{
  DwarfyCompilationUnitHeader *compilation_unit_header;
  DwarfyCompilationUnit *compilation_unit;
  unsigned char *line_number_program_ptr; 
  unsigned char *abbreviations_ptr;
  unsigned int unit_length;
  short unsigned int version;
  unsigned int debug_abbrev_offset;
  unsigned char architecture_address_size;

  *address = DEBUG_INFO;// + COMPILATION_UNIT_OFFSET;
  
  while((*address = DEBUG_INFO +  COMPILATION_UNIT_LENGTH) < DEBUG_INFO + DEBUG_INFO_SIZE)
  {
	compilation_unit = create_compilation_unit();
    
	compilation_unit_header = (DwarfyCompilationUnitHeader*)(*address);
        
    abbreviations_ptr = DEBUG_ABBREV + compilation_unit_header->abbreviations_offset;
	dwarfy_consume_abbreviations(compilation_unit,&abbreviations_ptr);

	COMPILATION_UNIT_LENGTH += compilation_unit_header->unit_length + 4;
	*address += sizeof(DwarfyCompilationUnitHeader);

	dwarfy_consume_DIEs(&compilation_unit->DIE_list,compilation_unit,address);
	line_number_program_ptr = DEBUG_LINE + LINE_NUMBER_PROGRAM_OFFSET;
	dwarfy_consume_line_numbers(compilation_unit,&line_number_program_ptr);    
    dwarfy_load_source_code(compilation_unit);
    LIST_INSERT_HEAD(compilation_units,compilation_unit,linkage);
  }

}


void dwarfy_consume_DIEs(DwarfyDIEList_t *DIE_list,DwarfyCompilationUnit *compilation_unit,unsigned char **address)
{
  DwarfyDIE *DIE;
  DwarfyAbbreviation *abbreviation;
  DwarfyAbbreviation match;
  DwarfyAttributeSpecList_t *attribute_spec_list;
  DwarfyAttributeSpec *spec;
  DwarfyFunction *function;
  unsigned long int abbreviation_code;
  unsigned long int size;
  int z = 1;
  function = 0;
  unsigned long int function_address;
  int DIE_is_function;
  char function_name[256];
  unsigned short language_code;
  
  for(;;)
  {
    if(0 == (abbreviation_code = dwarfy_consume_unsigned_LEB128(address)))
      return;
    
    if(*address - DEBUG_INFO >= COMPILATION_UNIT_LENGTH)
      return;
      
    DIE = malloc(sizeof(DwarfyDIE));
    DIE->abbreviation_code = abbreviation_code;
    LIST_INIT(&DIE->children);
    match.code = abbreviation_code;

    abbreviation = RB_FIND(DwarfyAbbreviationTree,&compilation_unit->abbreviations,&match);
    spec = LIST_FIRST(&abbreviation->specs);
    DIE_is_function = 0;
    
    while(spec)
    {
      if(abbreviation->tag == DW_TAG_subprogram)
      {
          
        DIE_is_function = 1;
        if(spec->name == DW_AT_low_pc)
          function_address = (**((unsigned long int**)address)) - DWARFY_ELF_BASE_ADDRESS + DWARFY_ELF_RUNTIME_ADDRESS;
        else if(spec->name == DW_AT_name)
          strcpy(function_name,((char*)DEBUG_STR) + **((unsigned int**)address));
      
          
      }
      else if(abbreviation->tag == DW_TAG_compile_unit)
      {
        if(spec->name == DW_AT_stmt_list)
        {
          LINE_NUMBER_PROGRAM_OFFSET = **((unsigned int**)address);
        }
        else if(spec->name == DW_AT_language)
        {
          language_code = **((unsigned char**)address);
          if(language_code != DW_LANG_C99 && language_code != DW_LANG_C89 && language_code != DW_LANG_C)
          {
             fprintf(stderr,"[valve] DWARF error: module was not written in C99, C89, or C.\n");
             exit(1);
          }
        }
        
      }
      
      switch(spec->form)
      {
        case DW_FORM_data1:
        {
          (*address)++;
          break;
        }
        case DW_FORM_data2:
        {
          (*address) += 2;
          break;
        }
        case DW_FORM_data4:
        {
          (*address) += 4;
          break;
        }
        case DW_FORM_data8:
        {
          (*address) += 8;
          break;
        }
        case DW_FORM_sdata:
        {
          dwarfy_consume_signed_LEB128((unsigned char**)address);
          break;
        }
        case DW_FORM_udata:
        {
          dwarfy_consume_unsigned_LEB128(address);
          break;
        }
        case DW_FORM_ref1:
        {
          (*address)++;
          break;
        }
        case DW_FORM_ref2:
        {
          (*address) += 2;
          break;
        }
        case DW_FORM_ref4:
        {
          (*address) += 4;
          break;
        }
        case DW_FORM_ref8:
        {
          (*address) += 8;
          break;
        }
        case DW_FORM_flag:
        {
          (*address)++;
          break;
        }
        case DW_FORM_flag_present:
        {
          break;
        }
        case DW_FORM_addr:
        {
          (*address) += 8;
          break;
        }
        case DW_FORM_sec_offset:
        {
          (*address) += 4;
          break;
        }
        case DW_FORM_strp:
        {
          (*address) += 4;
          break;
        }
        case DW_FORM_string:
        {
          (*address) += strlen((char*)*address) + 1;
          break;
        }
        case DW_FORM_exprloc:
        {
          size = dwarfy_consume_unsigned_LEB128(address);
          (*address) += size;
          break;
        }
	case DW_FORM_block1:
	{
	  size = **address;
	  (*address) += size + 1;
	  break;
	}
	default:
	{
	  exit(1);
	}
      }
      spec = LIST_NEXT(spec,linkage);
    }
    
    if(DIE_is_function)
    {
          function = malloc(sizeof(DwarfyFunction));
          function->address = function_address;
          function->name = malloc(256);
          strcpy(function->name,function_name);
          RB_INSERT(DwarfyFunctionTree,&compilation_unit->functions,function);
    }
    
    function = 0;
    DIE_is_function = 0;
    function_name[0] = 0;
    
    if(abbreviation->has_children)
      dwarfy_consume_DIEs(&DIE->children,compilation_unit,address);
    
    LIST_INSERT_HEAD(DIE_list,DIE,linkage);
    
  }
}

void dwarfy_consume_abbreviations(DwarfyCompilationUnit *compilation_unit,unsigned char **address)
{
  DwarfyAbbreviation *abbreviation;
  
  while((abbreviation = dwarfy_consume_abbreviation_header(address)))
  {
    dwarfy_consume_abbreviation_attribute_specs(abbreviation,address);
    RB_INSERT(DwarfyAbbreviationTree,&compilation_unit->abbreviations,abbreviation);
    
  }
}

DwarfyAbbreviation *dwarfy_consume_abbreviation_header(unsigned char **address)
{
  DwarfyAbbreviation *abbreviation;
  unsigned long int abbreviation_code;
  unsigned long int tag;
 
  abbreviation_code = dwarfy_consume_unsigned_LEB128(address);
  
  if(abbreviation_code == 0)
    return 0;
  
  abbreviation = malloc(sizeof(DwarfyAbbreviation));
  abbreviation->code = abbreviation_code;
   
  tag = dwarfy_consume_unsigned_LEB128(address);
  abbreviation->tag = tag;
  
  if(**address == DW_CHILDREN_yes)
  {
    abbreviation->has_children = 1;
  }
  else if(**address == DW_CHILDREN_no)
  {
    abbreviation->has_children = 0;
  }
  
  abbreviation->num_items = 0;
  
  (*address)++;
  
  return abbreviation;
}

void dwarfy_consume_abbreviation_attribute_specs(DwarfyAbbreviation *abbreviation,unsigned char **address)
{
  unsigned long int attribute_name, attribute_form;
  DwarfyAttributeSpec *spec;
  DwarfyAttributeSpec *tail;
  tail = 0;
  LIST_INIT(&abbreviation->specs);
  for(;;)
  {
    attribute_name = dwarfy_consume_unsigned_LEB128(address);
    attribute_form = dwarfy_consume_unsigned_LEB128(address);
    if(attribute_name == 0 && attribute_form == 0)
      return;
 
    spec = malloc(sizeof(DwarfyAttributeSpec));
    spec->name = attribute_name;
    spec->form = attribute_form;
    
    if(LIST_EMPTY(&abbreviation->specs))
      LIST_INSERT_HEAD(&abbreviation->specs,spec,linkage);
    else
      LIST_INSERT_AFTER(tail,spec,linkage);
    tail = spec;
    
    abbreviation->num_items++;
  }
}

void dwarfy_consume_line_numbers(DwarfyCompilationUnit *compilation_unit,unsigned char **address)
{
  DwarfyLineNumberHeader *line_number_header;
  char is_stmt;

  line_number_header = dwarfy_consume_line_number_header(compilation_unit,address);
  dwarfy_execute_line_number_program(compilation_unit,line_number_header,address);
  
  return;
}

DwarfyLineNumberHeader *dwarfy_consume_line_number_header(DwarfyCompilationUnit *compilation_unit,unsigned char **address)
{
  DwarfyLineNumberHeader *line_number_header, *result;
  char *string;
  unsigned char *instruction_pointer;
  unsigned long int index,modification,length;

  line_number_header = (DwarfyLineNumberHeader*)*address;
  
  *address = ((unsigned char*)(line_number_header + 1)) + 12;
  while(**address != '\0')
  {
    compilation_unit->num_include_paths++;
    compilation_unit->include_paths[compilation_unit->num_include_paths - 1] = malloc(strlen((char*)(*address)) + 1);
    strcpy(compilation_unit->include_paths[compilation_unit->num_include_paths - 1],(char*)(*address));
    *address += strlen((char*)(*address)) + 1;
  }
  (*address)++;
  
  while(**address != '\0')
  {
    compilation_unit->num_file_names++;
    compilation_unit->file_names[compilation_unit->num_file_names - 1] = malloc(strlen((char*)(*address)) + 1);
    strcpy(compilation_unit->file_names[compilation_unit->num_file_names - 1],(char*)(*address));
    *address += strlen((char*)(*address)) + 1;
    index = dwarfy_consume_unsigned_LEB128(address);
    compilation_unit->directory_indices[compilation_unit->num_file_names - 1] = index;
    modification = dwarfy_consume_unsigned_LEB128(address);
    length = dwarfy_consume_unsigned_LEB128(address);
  }
  (*address)++;


  instruction_pointer = ((unsigned char*)(&line_number_header->minimum_instruction_length)) + line_number_header->header_length;
  result = malloc(sizeof(DwarfyLineNumberHeader));// (DwarfyLineNumberHeader*)*address;
  memcpy(result,line_number_header,sizeof(DwarfyLineNumberHeader));
  
  return result;
  
}

void dwarfy_init_line_number_state_machine(DwarfyLineNumberStateMachine *state_machine,DwarfyLineNumberHeader* line_number_header)
{
  state_machine->address = 0;
  state_machine->file = 1;
  state_machine->line = 1;
  state_machine->column = 0;
  state_machine->is_stmt = line_number_header->default_is_stmt;
  state_machine->basic_block = 0;
  state_machine->end_sequence = 0;
}

void dwarfy_execute_line_number_program(DwarfyCompilationUnit *compilation_unit,DwarfyLineNumberHeader *line_number_header,unsigned char **address)
{
  unsigned char opcode;
  unsigned char adjusted_opcode;
  unsigned long int operation_size;
  char *file_name;
  unsigned long int directory_index;
  unsigned long int modification_time;
  unsigned long int size;
  DwarfyLineNumberStateMachine state_machine;
  
  dwarfy_init_line_number_state_machine(&state_machine,line_number_header);
  
  for(;;)
  {
    opcode = **address;
    (*address)++;
    
    if(opcode == 0)
    {
      // extended opcode
      
      dwarfy_consume_unsigned_LEB128(address);
      
      opcode = **address;
      (*address)++;
      
      switch(opcode)
      {
        case DW_LNE_end_sequence:
          state_machine.end_sequence = 1;
          dwarfy_line_number_state_machine_out(&state_machine,compilation_unit);
          return;
        case DW_LNE_set_address:
          state_machine.address = **((unsigned long int**)address);
          (*address) += DWARFY_ARCHITECTURE_ADDRESS_SIZE ;
          break;
        case DW_LNE_define_file:
        {
          file_name = (char*)*address;
          *address += strlen((char*)(*address)) + 1;
          
          directory_index = dwarfy_consume_unsigned_LEB128(address);
          modification_time = dwarfy_consume_unsigned_LEB128(address);
          size = dwarfy_consume_unsigned_LEB128(address);
          break;
        }
        case DW_LNE_set_discriminator:
          dwarfy_consume_unsigned_LEB128(address);
          break;
      }
    }
    else if(opcode < line_number_header->opcode_base) // standard opcode
    {
      switch(opcode)
      {
        case DW_LNS_copy:
          dwarfy_line_number_state_machine_out(&state_machine,compilation_unit);
          state_machine.basic_block = 0;
          break;
        case DW_LNS_advance_pc:
          state_machine.address += dwarfy_consume_unsigned_LEB128(address);
          break;
        case DW_LNS_advance_line:
        {
          state_machine.line += dwarfy_consume_signed_LEB128(address);
          break;
         }
        case DW_LNS_set_file:
        {
          state_machine.file = dwarfy_consume_unsigned_LEB128(address);
          break;
	}
        case DW_LNS_set_column:
          state_machine.column = dwarfy_consume_unsigned_LEB128(address);
          break;
        case DW_LNS_negate_stmt:
          state_machine.is_stmt = !state_machine.is_stmt;
          break;
        case DW_LNS_set_basic_block:
          state_machine.basic_block = 1;
          break;
        case DW_LNS_const_add_pc:
          adjusted_opcode = 255 - line_number_header->opcode_base;
          state_machine.address += adjusted_opcode / line_number_header->line_range;
          dwarfy_line_number_state_machine_out(&state_machine,compilation_unit);
          break;
        case DW_LNS_fixed_advance_pc:
          state_machine.address += *((unsigned short*)(*address));
          (*address) += 2;
          break;
      }
    }
    else // special opcode
    {
      adjusted_opcode = opcode - line_number_header->opcode_base;
      state_machine.address += adjusted_opcode / line_number_header->line_range;
      state_machine.line += line_number_header->line_base + (adjusted_opcode % line_number_header->line_range);
      dwarfy_line_number_state_machine_out(&state_machine,compilation_unit);
    }
  }
}

void dwarfy_line_number_state_machine_out(DwarfyLineNumberStateMachine *state_machine,DwarfyCompilationUnit *compilation_unit)
{
  DwarfyObjectRecord *object_record;
  DwarfyObjectRecord match_object_record;
  DwarfySourceRecord *source_record;
  
  match_object_record.address = state_machine->address - DWARFY_ELF_BASE_ADDRESS + DWARFY_ELF_RUNTIME_ADDRESS;
  
  if(0 == (object_record = RB_FIND(DwarfyObjectRecordTree,&compilation_unit->line_numbers,&match_object_record)))
  {
    object_record = malloc(sizeof(DwarfyObjectRecord));
    
    LIST_INIT(&object_record->source_records);
    object_record->address = state_machine->address - DWARFY_ELF_BASE_ADDRESS + DWARFY_ELF_RUNTIME_ADDRESS;
    RB_INSERT(DwarfyObjectRecordTree,&compilation_unit->line_numbers,object_record);
  }
  
  source_record = malloc(sizeof(DwarfySourceRecord));
  
  source_record->file = state_machine->file;
  source_record->line_number = state_machine->line;
  source_record->column = state_machine->column;
  
  LIST_INSERT_HEAD(&object_record->source_records,source_record,linkage);
  
}

int file_num_lines(char *file_name)
{
  FILE *file;
  int file_size;
  char *block;
  int num_lines;
  int i;
  i = 0;
  num_lines = 0;
  file = fopen(file_name,"rb");
  if(file == 0)
    return 0;
  
  fseek(file,0,SEEK_END);
  file_size = ftell(file);
  fseek(file,0,SEEK_SET);
  block = malloc(file_size);
  fread(block,file_size,1,file);
  
  while(i < file_size)
  {
    if(block[i] == 0xA)
      num_lines++;
    i++;
  }
  
  fclose(file);
  free(block);
  
  return num_lines;
}

void dwarfy_load_source_code(DwarfyCompilationUnit *compilation_unit)
{
  DwarfySourceCode *source_code;
  char *source_line;
  unsigned int line_number;
  int i,j,k;
  int num_lines;
  char buffer[1024];
  FILE *file;
  unsigned long int file_size;
  char *block;
  
  for(i = 0; i < compilation_unit->num_file_names; i++)
  {
    strncpy(buffer,compilation_unit->include_paths[compilation_unit->directory_indices[i]],512);
    strncat(buffer,compilation_unit->file_names[i],511);
    
    num_lines = file_num_lines(buffer);
    
    file = fopen(buffer,"rb");
    if(!file)
      continue;    
    
    source_code = malloc(sizeof(DwarfySourceCode));
    
    source_code->file_name = malloc(strlen(compilation_unit->file_names[i]) + 1);
    strcpy(source_code->file_name,compilation_unit->file_names[i]);
    source_code->source_code = malloc(num_lines * sizeof(char*));
	source_code->num_lines = num_lines;
    
    fseek(file,0,SEEK_END);
    file_size = ftell(file);
    fseek(file,0,SEEK_SET);
    block = malloc(file_size);
    fread(block,file_size,1,file);
    
    line_number = 0;
    j = 0;
    for(j = 0; j < file_size; j++)
    {
      line_number++;
      k = j;
      while(block[j] != 0xA && j < file_size)
        j++;
      source_line = malloc(j - k + 1);
      memcpy(source_line,block + k,j-k);
      source_line[j - k] = '\0';
      source_code->source_code[line_number - 1] = source_line;
    }
    RB_INSERT(DwarfySourceCodeTree,&compilation_unit->source_code,source_code);
    
    free(block);
    fclose(file);
  }
}

int dwarfy_compare_integers(unsigned long int a, unsigned long int b)
{
  if(a < b)
    return -1;
  else if(a > b)
    return 1;
  else
    return 0;
}


long int dwarfy_consume_signed_LEB128(unsigned char **address)
{
  long int result = 0; 
  long int shift = 0; 
  long int size = DWARFY_ARCHITECTURE_ADDRESS_SIZE * 8; 
  unsigned long int byte = 0;
  int i;
  
  for(i = 0; 1; i++) 
  {
    byte = (unsigned long int)((*address)[i]);
    result |= ((byte & 0x7f) << shift);
    shift += 7;
    if ((byte & 0x80) == 0) 
      break; 
  } 
  if ((shift < size) && (byte & 0x40))
    result |= - (1 << shift);
    
  *address += i + 1;
  return result;
}

unsigned long int dwarfy_consume_unsigned_LEB128(unsigned char **address)
{
  unsigned long int result = 0;
  unsigned long int shift = 0;
  unsigned long int byte = 0;
  int i; 
  for(i = 0; 1; i++) 
  {
     byte = (unsigned long int)((*address)[i]);
     result |= ((byte & 0x7F) << shift); 
     if ((byte & 0x80) == 0) 
          break; 
     shift += 7;
  }
  *address += i + 1;
  return result;
}

char *dwarfy_tag_to_string(unsigned long int tag)
{
  switch(tag)
  {
    case 0x01:
    {
      return "array_type";
    }
    case 0x02:
    {
      return "class_type";
    }
    case 0x03:
    {
      return "entry_point";
    }
    case 0x4:
    {
      return "enumeration_type";
    }
    case 0x05:
    {
      return "formal_parameter";
    }
    case 0x08:
    {
      return "imported_declaration";
    }
    case 0x0a:
    {
      return "label";
    }
    case 0x0b:
    {
      return "lexical_block";
    }
    case 0x0d:
    {
      return "member";
    }
    case 0x0f:
    {
      return "pointer_type";
    }
    case 0x10:
    {
      return "reference_type";
    }
    case 0x11:
    {
      return "compile_unit";
    }
    case 0x12:
    {
      return "string_type";
    }
    case 0x13:
    {
      return "structure_type";
    }
    case 0x15:
    {
      return "subroutine_type";
    }
    case 0x16:
    {
      return "typedef";
    }
    case 0x17:
    {
      return "union_type";
    }
    case 0x18:
    {
      return "unspecified_parameters";
    }
    case 0x19:
    {
      return "variant";
    }
    case 0x1a:
    {
      return "common_block";
    }
    case 0x1b:
    {
      return "common_inclusion";
    }
    case 0x1c:
    {
      return "inheritance";
    }
    case 0x1d:
    {
      return "inlined_subroutine";
    }
    case 0x1e:
    {
      return "module";
    }
    case 0x1f:
    {
      return "ptr_to_member_type";
    }
    case 0x20:
    {
      return "set_type";
    }
    case 0x21:
    {
      return "subrange_type";
    }
    case 0x22:
    {
      return "with_stmt";
    }
    case 0x23:
    {
      return "access_declaration";
    }
    case 0x24:
    {
      return "base_type";
    }
    case 0x25:
    {
      return "catch_block";
    }
    case 0x26:
    {
      return "const_type";
    }
    case 0x27:
    {
      return "constant";
    }
    case 0x28:
    {
      return "enumerator";
    }
    case 0x29:
    {
      return "file_type";
    }
    case 0x2a:
    {
      return "friend";
    }
    case 0x2b:
    {
      return "namelist";
    }
    case 0x2c:
    {
      return "namelist_item";
    }
    case 0x2d:
    {
      return "packed_type";
    }
    case 0x2e:
    {
      return "subprogram";
    }
    case 0x2f:
    {
      return "template_type_parameter";
    }
    case 0x30:
    {
      return "template_value_parameter";
    }
    case 0x31:
    {
      return "thrown_type";
    }
    case 0x32:
    {
      return "try_block";
    }
    case 0x33:
    {
      return "variant_part";
    }
    case 0x34:
    {
      return "variable";
    }
    case 0x35:
    {
      return "volatile_type";
    }
    case 0x36:
    {
      return "dwarf_procedure";
    }
    case 0x37:
    {
      return "restrict_type";
    }
    case 0x38:
    {
      return "interface_type";
    }
    case 0x39:
    {
      return "namespace";
    }
    case 0x3a:
    {
      return "imported_module";
    }
    case 0x3b:
    {
      return "unspecified_type";
    }
    case 0x3c:
    {
      return "partial_unit";
    }
    case 0x3d:
    {
      return "imported_unit";
    }
    case 0x3f:
    {
      return "condition";
    }
    case 0x40:
    {
      return "shared_type";
    }
    case 0x41:
    {
      return "type_unit";
    }
    case 0x42:
    {
      return "rvalue_reference_type";
    }
    case 0x43:
    {
      return "template_alias";
    }
    default:
    {
      if(tag >= 0x4080 && tag <= 0xffff)
        return "USER_DEFINED_TAG";
      else
        return "UNKNOWN_TAG";
    }
  }
}

char *dwarfy_attribute_to_string(unsigned long int attribute)
{
  switch(attribute)
  {
    case 0x01:
    {
      return "sibling";
    }
    case 0x02:
    {
      return "location";
    }
    case 0x03:
    {
      return "name";
    }
    case 0x09:
    {
      return "ordering";
    }
    case 0x0b:
    {
      return "byte_size";
    }
    case 0x0c:
    {
      return "bit_offset";
    }
    case 0x0d:
    {
      return "bit_size";
    }
    case 0x10:
    {
      return "stmt_list";
    }
    case 0x11:
    {
      return "low_pc";
    }
    case 0x12:
    {
      return "high_pc";
    }
    case 0x13:
    {
      return "language";
    }
    case 0x15:
    {
      return "discr";
    }
    case 0x16:
    {
      return "discr_value";
    }
    case 0x17:
    {
      return "visibility";
    }
    case 0x18:
    {
      return "import";
    }
    case 0x19:
    {
      return "string_length";
    }
    case 0x1a:
    {
      return "common_reference";
    }
    case 0x1b:
    {
      return "comp_dir";
    }
    case 0x1c:
    {
      return "const_value";
    }
    case 0x1d:
    {
      return "containing_type";
    }
    case 0x1e:
    {
      return "default_value";
    }
    case 0x20:
    {
      return "inline";
    }
    case 0x21:
    {
      return "is_optional";
    }
    case 0x22:
    {
      return "lower_bound";
    }
    case 0x25:
    {
      return "producer";
    }
    case 0x27:
    {
      return "prototyped";
    }
    case 0x2a:
    {
      return "return_addr";
    }
    case 0x2c:
    {
      return "start_scope";
    }
    case 0x2e:
    {
      return "bit_stride";
    }
    case 0x2f:
    {
      return "upper_bound";
    }
    case 0x31:
    {
      return "abstract_origin";
    }
    case 0x32:
    {
      return "accesibility";
    }
    case 0x33:
    {
      return "address_class";
    }
    case 0x34:
    {
      return "artificial";
    }
    case 0x35:
    {
      return "base_types";
    }
    case 0x36:
    {
      return "calling_convention";
    }
    case 0x37:
    {
      return "count";
    }
    case 0x38:
    {
      return "data_member_location";
    }
    case 0x39:
    {
      return "decl_column";
    }
    case 0x3a:
    {
      return "decl_file";
    }
    case 0x3b:
    {
      return "decl_line";
    }
    case 0x3c:
    {
      return "declaration";
    }
    case 0x3d:
    {
      return "discr_list";
    }
    case 0x3e:
    {
      return "encoding";
    }
    case 0x3f:
    {
      return "external";
    }
    case 0x40:
    {
      return "frame_base";
    }
    case 0x41:
    {
      return "friend";
    }
    case 0x42:
    {
      return "identifier_case";
    }
    case 0x43:
    {
      return "macro_info";
    }
    case 0x44:
    {
      return "namelist_item";
    }
    case 0x45:
    {
      return "priority";
    }
    case 0x46:
    {
      return "segment";
    }
    case 0x47:
    {
      return "specification";
    }
    case 0x48:
    {
      return "static_link";
    }
    case 0x49:
    {
      return "type";
    }
    case 0x4a:
    {
      return "use_location";
    }
    case 0x4b:
    {
      return "variable_parameter";
    }
    case 0x4c:
    {
      return "virtuality";
    }
    case 0x4d:
    {
      return "vtable_elem_location";
    }
    case 0x4e:
    {
      return "allocated";
    }
    case 0x4f:
    {
      return "associated";
    }
    case 0x50:
    {
      return "data_location";
    }
    case 0x51:
    {
      return "byte_stride";
    }
    case 0x52:
    {
      return "entry_pc";
    }
    case 0x53:
    {
      return "use_utf8";
    }
    case 0x54:
    {
      return "extension";
    }
    case 0x55:
    {
      return "ranges";
    }
    case 0x56:
    {
      return "trampoline";
    }
    case 0x57:
    {
      return "call_column";
    }
    case 0x58:
    {
      return "call_file";
    }
    case 0x59:
    {
      return "call_line";
    }
    case 0x5a:
    {
      return "description";
    }
    case 0x5b:
    {
      return "binary_scale";
    }
    case 0x5c:
    {
      return "decimal_scale";
    }
    case 0x5d:
    {
      return "small";
    }
    case 0x5e:
    {
      return "decimal_sign";
    }
    case 0x5f:
    {
      return "digit_count";
    }
    case 0x60:
    {
      return "picture_string";
    }
    case 0x61:
    {
      return "mutable";
    }
    case 0x62:
    {
      return "threads_scaled";
    }
    case 0x63:
    {
      return "explicit";
    }
    case 0x64:
    {
      return "object_pointer";
    }
    case 0x65:
    {
      return "endianity";
    }
    case 0x66:
    {
      return "elemental";
    }
    case 0x67:
    {
      return "pure";
    }
    case 0x68:
    {
      return "recursive";
    }
    case 0x69:
    {
      return "signature";
    }
    case 0x6a:
    {
      return "main_subprogram";
    }
    case 0x6b:
    {
      return "data_bit_offset";
    }
    case 0x6c:
    {
      return "const_expr";
    }
    case 0x6d:
    {
      return "enum_class";
    }
    case 0x6e:
    {
      return "linkage_name";
    }
    default:
    {
      if(attribute >= 0x2000 && attribute <= 0x3fff)
        return "USER_DEFINED_ATTRIBUTE";
      else
        return "UNRECOGNISED_ATTRIBUTE";
    }
  }
  
}

char *dwarfy_form_to_string(unsigned long int form)
{
  switch(form)
  {
    case 0x01:
    {
      return "addr";
    }
    case 0x03:
    {
      return "block2";
    }
    case 0x04:
    {
      return "block4";
    }
    case 0x05:
    {
      return "data2";
    }
    case 0x06:
    {
      return "data4";
    }
    case 0x07:
    {
      return "data8";
    }
    case 0x08:
    {
      return "string";
    }
    case 0x09:
    {
      return "block";
    }
    case 0x0a:
    {
      return "block1";
    }
    case 0x0b:
    {
      return "data1";
    }
    case 0x0c:
    {
      return "flag";
    }
    case 0x0d:
    {
      return "sdata";
    }
    case 0x0e:
    {
      return "strp";
    }
    case 0x0f:
    {
      return "udata";
    }
    case 0x10:
    {
      return "ref_addr";
    }
    case 0x11:
    {
      return "ref1";
    }
    case 0x12:
    {
      return "ref2";
    }
    case 0x13:
    {
      return "ref4";
    }
    case 0x14:
    {
      return "ref8";
    }
    case 0x15:
    {
      return "ref_udata";
    }
    case 0x16:
    {
      return "indirect";
    }
    case 0x17:
    {
      return "sec_offset";
    }
    case 0x18:
    {
      return "exprloc";
    }
    case 0x19:
    {
      return "flag_present";
    }
    case 0x20:
    {
      return "ref_sig8";
    }
    default:
    {
      return "UNRECOGNISED_FORM";
    }
  }
}


