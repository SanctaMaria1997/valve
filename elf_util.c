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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include "elf_util.h"

unsigned char *load_elf(char *file_name)
{
  unsigned char *block;
  unsigned int file_size;
  FILE *f;

  block = 0;
  f = fopen(file_name,"rb");
  if(f == 0)
    return 0;
  fseek(f,0,SEEK_END);
  file_size = ftell(f);
  fseek(f,0,SEEK_SET);
  block = malloc(file_size);
  fread(block,file_size,1,f);
  fclose(f);
  
  return block;
}

unsigned long int get_elf_base_address(unsigned char *elf)
{
  char *section_name;
  Elf64_Ehdr *elf_header;
  Elf64_Phdr *program_header;
  unsigned short num_program_headers;
  unsigned long int lowest_address = 0xffffffffffffffff;
  int i;
    
  elf_header = (Elf64_Ehdr*)elf;
  program_header = (Elf64_Phdr*)(elf + elf_header->e_phoff);
  num_program_headers = elf_header->e_phnum;
  
  for(i = 0; i < num_program_headers; i++)
  { 
    if(program_header[i].p_type == PT_LOAD && program_header[i].p_vaddr < lowest_address)
        lowest_address = program_header[i].p_vaddr;
  }
  
  return lowest_address;
}

unsigned long int get_elf_symbol(unsigned char *elf,char *name)
{
  char *section_name;
  Elf64_Ehdr *elf_header;
  Elf64_Shdr *section_header;
  Elf64_Sym *symbol;
  unsigned long int num_sections;
  unsigned long int section_header_size;
  unsigned long int section_names_index;
  unsigned long int symtab_offset;
  unsigned long int symtab_size;
  unsigned long int strtab_offset;
  unsigned long int strtab_size;
  unsigned long int address;
  int i;
  
  address = 0;
  
  elf_header = (Elf64_Ehdr*)elf;
  section_header = (Elf64_Shdr*)(elf + elf_header->e_shoff);
  num_sections = elf_header->e_shnum;
  section_header_size = elf_header->e_shentsize;
  section_names_index = elf_header->e_shstrndx;
  if(section_names_index == SHN_XINDEX)
    section_names_index = section_header->sh_link;
  
  for(i = 0; i < num_sections; i++)
  { 
    section_name = (char*)(elf + section_header[section_names_index].sh_offset + section_header[i].sh_name);
    
    if(!strcmp(section_name,".symtab"))
    {
      symtab_offset = section_header[i].sh_offset;
      symtab_size = section_header[i].sh_size;
    }
    else if(!strcmp(section_name,".strtab"))
    {
      strtab_offset = section_header[i].sh_offset;
      strtab_size = section_header[i].sh_size;
    }
  }

  symbol = (Elf64_Sym*)(elf + symtab_offset);
  
  while(((unsigned char*)symbol) < elf + symtab_offset + symtab_size)
  {
    if(!strcmp(name,(const char*)(elf + strtab_offset + symbol->st_name)))
    {
        address = symbol->st_value;
        break;
    }
    symbol++;   
  }
  
  return address;
  
}

unsigned long int get_elf_relocation(unsigned char *elf,char *name)
{
  char *section_name;
  Elf64_Ehdr *elf_header;
  Elf64_Shdr *section_header;
  Elf64_Sym *symbol;
  unsigned long int num_sections;
  unsigned long int section_header_size;
  unsigned long int section_names_index;
  unsigned long int dynsym_offset;
  unsigned long int dynsym_size;
  unsigned long int dynstr_offset;
  unsigned long int dynstr_size;
  unsigned long int rela_offset;
  unsigned long int rela_size;
  int success = 0;
  
  elf_header = (Elf64_Ehdr*)elf;
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
    if(!strcmp(section_name,".dynsym"))
    {
      dynsym_offset = section_header[i].sh_offset;
      dynsym_size = section_header[i].sh_size;
    }
    else if(!strcmp(section_name,".dynstr"))
    {
      dynstr_offset = section_header[i].sh_offset;
      dynstr_size = section_header[i].sh_size;
    }
    else if(!strcmp(section_name,".rela.plt"))
    {
      rela_offset = section_header[i].sh_offset;
      rela_size = section_header[i].sh_size;
    }
  }
  Elf64_Rela *relocation = (Elf64_Rela*)(elf + rela_offset);
  char *symbol_name;

  while(((unsigned char*)relocation) < elf + rela_offset + rela_size)
  {
    symbol = ((Elf64_Sym*)(elf + dynsym_offset)) + ELF64_R_SYM(relocation->r_info);
    symbol_name = (char*)(elf + dynstr_offset + symbol->st_name);
    if(!strcmp(symbol_name,name))
    {
      success = 1;
      break;
    }
    relocation++;
  }  
  
  if(success)
    return relocation->r_offset;
  else
    return 0;

}
