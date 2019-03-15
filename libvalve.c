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

#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "dwarfy.h"
#include "valve.h"
#include "libvalve.h"
#include "valve_util.h"

RB_GENERATE(AllocationPointTree,AllocationPoint,AllocationPointLinks,compare_allocation_points);
RB_GENERATE(MemoryBlockTree,MemoryBlock,MemoryBlockLinks,compare_memory_blocks);

long int LIBVALVE_NUM_ALLOCS;
long int LIBVALVE_NUM_MALLOCS;
long int LIBVALVE_NUM_CALLOCS;
long int LIBVALVE_NUM_REALLOCS;
long int LIBVALVE_NUM_FREES;

int VALVE_INSTANCE_COUNTER;
int LIBVALVE_INIT_COUNTER;

LibvalveSharedMem *LIBVALVE_SHARED_MEM;
long int LIBVALVE_REGION_BASE[LIBVALVE_MAX_NUM_REGIONS];
long int LIBVALVE_NUM_REGIONS;

AllocationPointTree_t ALLOCATION_POINTS;

DWARF_DATAList_t DWARFY_PROGRAM;

void leak_report(void);

int compare_allocation_points(AllocationPoint *a1,AllocationPoint *a2)
{
  return a1->address - a2->address;
}

int compare_memory_blocks(MemoryBlock *mb1,MemoryBlock *mb2)
{
  return mb1->address - mb2->address;
}

void  __attribute__((constructor)) libvalve_init()
{
  int shmid;
  int i = 0;
  char name[256];
  
  LIBVALVE_NUM_ALLOCS = LIBVALVE_NUM_MALLOCS = LIBVALVE_NUM_CALLOCS = LIBVALVE_NUM_REALLOCS = LIBVALVE_NUM_FREES = 0;
  
  RB_INIT(&ALLOCATION_POINTS);
  
  shmid = shmget(ftok("/usr/local/lib/libvalve.so",1),LIBVALVE_MAX_NUM_LIBRARIES * sizeof(Library),0666);
  LIBVALVE_SHARED_MEM =shmat(shmid,0,0);
  
  raise(SIGTRAP);
  
  while(strlen(LIBVALVE_SHARED_MEM->libraries[i].name))
  {
    strcpy(name,file_part(LIBVALVE_SHARED_MEM->libraries[i].name));
    LIBVALVE_SHARED_MEM->libraries[i].dwarf = load_dwarf(find_file(name,"."),LIBVALVE_SHARED_MEM->libraries[i].base_address);
    i++;
  }
  
}

AllocationPoint *create_allocation_point(long int address)
{
  AllocationPoint *allocation_point;
  
  allocation_point = malloc(sizeof(AllocationPoint));
  memset(allocation_point,0,sizeof(AllocationPoint));
  RB_INIT(&allocation_point->memory_blocks);
  allocation_point->address = address;
  RB_INSERT(AllocationPointTree,&ALLOCATION_POINTS,allocation_point);
  
  return allocation_point;
}

MemoryBlock *create_memory_block(unsigned long int address,size_t size,AllocationPoint *allocation_point)
{
  MemoryBlock *memory_block;
  
  memory_block = malloc(sizeof(MemoryBlock));
  memory_block->address = address;
  memory_block->size = size;
  memory_block->allocation_point = allocation_point;
  
  RB_INSERT(MemoryBlockTree,&allocation_point->memory_blocks,memory_block);
  
  return memory_block;
}

void *malloc_wrapper(size_t size)
{  
  void *result;
  long int return_address = 0;
  long int allocation_address;
  AllocationPoint match_allocation_point;
  AllocationPoint *allocation_point;
  MemoryBlock *memory_block;
  
  __asm__
  (
    "movq 8(%%rbp), %%rax\n"
    "movq %%rax, %0\n"
    : [return_address] "=r"(return_address)
    :
    : "rax"
  );
  
  match_allocation_point.address = return_address;

  if(0 == (allocation_point = RB_FIND(AllocationPointTree,&ALLOCATION_POINTS,&match_allocation_point)))
    allocation_point = create_allocation_point(match_allocation_point.address);
  
  allocation_point->current_num_allocations++;
  allocation_point->current_bytes_allocated += size;
  allocation_point->total_num_allocations++;
  allocation_point->total_bytes_allocated += size;

  result = malloc(size);
  
  memory_block = create_memory_block((unsigned long int)result,size,allocation_point);

  LIBVALVE_NUM_ALLOCS++;
  LIBVALVE_NUM_MALLOCS++;
  
  return result;
}

void *calloc_wrapper(size_t num,size_t size)
{
  void *result;
  long int return_address = 0;
  long int allocation_address;
  AllocationPoint match_allocation_point;
  AllocationPoint *allocation_point;
  MemoryBlock *memory_block;
  
  __asm__
  (
    "movq 8(%%rbp), %%rax\n"
    "movq %%rax, %0\n"
    : [return_address] "=r"(return_address)
    :
    : "rax"
  );
  
  match_allocation_point.address = return_address;
  
  if(0 == (allocation_point = RB_FIND(AllocationPointTree,&ALLOCATION_POINTS,&match_allocation_point)))
    allocation_point = create_allocation_point(match_allocation_point.address);
  
  allocation_point->current_num_allocations++;
  allocation_point->current_bytes_allocated += num * size;
  allocation_point->total_num_allocations++;
  allocation_point->total_bytes_allocated += num * size;

  result = calloc(num,size);
  
  memory_block = create_memory_block((unsigned long int)result,num * size,allocation_point);

  LIBVALVE_NUM_ALLOCS++;
  LIBVALVE_NUM_CALLOCS++;

  return result;
}

void *realloc_wrapper(void *ptr,size_t size)
{
  void *result;
  long int return_address = 0;
  long int size_difference = 0;
  AllocationPoint match_allocation_point;
  AllocationPoint *allocation_point;
  MemoryBlock *memory_block,match_memory_block;
  match_memory_block.address = (unsigned long int)ptr;
  
  memory_block = 0;
  
  RB_FOREACH(allocation_point,AllocationPointTree,&ALLOCATION_POINTS)
  {
    if((memory_block = RB_FIND(MemoryBlockTree,&allocation_point->memory_blocks,&match_memory_block)))
      break;
  }
  
  if(memory_block == 0)
    return realloc(ptr,size);
  
  __asm__
  (
    "movq 8(%%rbp), %%rax\n"
    "movq %%rax, %0\n"
    : [return_address] "=r"(return_address)
    :
    : "rax"
  );
  
  match_allocation_point.address = return_address;
  
  if(0 == (allocation_point = RB_FIND(AllocationPointTree,&ALLOCATION_POINTS,&match_allocation_point)))
    allocation_point = create_allocation_point(match_allocation_point.address);
  
  if(allocation_point != memory_block->allocation_point)
  {
    RB_REMOVE(MemoryBlockTree,&memory_block->allocation_point->memory_blocks,memory_block);
    memory_block->allocation_point->current_bytes_allocated -= memory_block->size;
    memory_block->allocation_point->current_num_allocations--;
    RB_INSERT(MemoryBlockTree,&allocation_point->memory_blocks,memory_block);
  }
  
  allocation_point->current_bytes_allocated += size;
  allocation_point->total_bytes_allocated += size;
  allocation_point->current_num_allocations++;
  allocation_point->total_num_allocations++;

  result = realloc(ptr,size);
  
  memory_block->address = (unsigned long int)result;
  memory_block->size = size;
  memory_block->allocation_point = allocation_point;

  LIBVALVE_NUM_ALLOCS++;
  LIBVALVE_NUM_REALLOCS++;
  
  return result;
}

void free_wrapper(void *ptr)
{
  MemoryBlock match;
  MemoryBlock *memory_block;
  AllocationPoint *allocation_point;
  match.address = (unsigned long int)ptr;

  RB_FOREACH(allocation_point,AllocationPointTree,&ALLOCATION_POINTS)
  {
    if((memory_block = RB_FIND(MemoryBlockTree,&allocation_point->memory_blocks,&match)))
    {
      allocation_point->current_num_allocations--;
      allocation_point->current_bytes_allocated -= memory_block->size;
      RB_REMOVE(MemoryBlockTree,&allocation_point->memory_blocks,memory_block);
    }

  }
  
  LIBVALVE_NUM_FREES++;
  
  free(ptr);
}

__attribute__((destructor)) void libvalve_final()
{
  fprintf(stderr,"\n[libvalve] Memory usage summary:\n");
  fprintf(stderr,"[libvalve] Application allocated %ld block(s)\n",LIBVALVE_NUM_ALLOCS);
  fprintf(stderr,"[libvalve] (malloc: %ld, calloc: %ld, realloc: %ld)\n",LIBVALVE_NUM_MALLOCS,LIBVALVE_NUM_CALLOCS,LIBVALVE_NUM_REALLOCS);
  fprintf(stderr,"[libvalve] Application freed %ld block(s)\n\n",LIBVALVE_NUM_FREES);
  
  leak_report();
}

void leak_report()
{
  AllocationPoint *allocation_point;
  MemoryBlock *memory_block;

  DwarfyObjectRecord match_location_record;
  DwarfyFunction match_function;
  DwarfyFunction *nearest_function;
  DwarfySourceCode match_source_file;
  DwarfySourceCode *source_code;
  DwarfyCompilationUnit *compilation_unit;
  int matched;
  unsigned long int num_leaks;
  int line_number;
  unsigned long int elf_address = 0;
  char *name;
  int i;
  DwarfyObjectRecord *nearest_object_record;
  DwarfySourceRecord *nearest_source_record;
  
  fprintf(stderr,"[libvalve] Leak report:\n");
  
  num_leaks = 0;
  
  RB_FOREACH(allocation_point,AllocationPointTree,&ALLOCATION_POINTS)
  {
    if(allocation_point->current_num_allocations)
    {
      elf_address = allocation_point->address;// - (long int)info.dli_fbase;
      nearest_object_record = 0;
      matched = 0;
      compilation_unit = 0;
      
      do
      {
        elf_address--;
        match_location_record.address = elf_address;
        
        i = 0;
        while(strlen(LIBVALVE_SHARED_MEM->libraries[i].name))
        {
          if(LIBVALVE_SHARED_MEM->libraries[i].dwarf)
          {
            LIST_FOREACH(compilation_unit,&LIBVALVE_SHARED_MEM->libraries[i].dwarf->compilation_units,linkage)
            {
              if((nearest_object_record = RB_FIND(DwarfyObjectRecordTree,&compilation_unit->line_numbers,&match_location_record)))
              {
                nearest_source_record = LIST_FIRST(&nearest_object_record->source_records);
                matched = 1;
                break;
              }
            }
          }
          if(matched) break;
          i++;
        }
        
      } while(!matched);
      
      nearest_function = 0;
      
      do
      {
        elf_address--;
        match_function.address = elf_address;
        nearest_function = RB_FIND(DwarfyFunctionTree,&compilation_unit->functions,&match_function);
      } while (!nearest_function);

      if(allocation_point->current_bytes_allocated && allocation_point->current_num_allocations)
      {
        
        num_leaks += allocation_point->current_num_allocations;
        
        fprintf(stderr,"[libvalve] %s:%u [in function %s(...)]: %lu bytes leaked in %lu block(s)\n\n",compilation_unit->file_names[nearest_source_record->file - 1],nearest_source_record->line_number,nearest_function->name,allocation_point->current_bytes_allocated,allocation_point->current_num_allocations); 
        match_source_file.file_name = compilation_unit->file_names[nearest_source_record->file - 1];
        
        source_code = RB_FIND(DwarfySourceCodeTree,&compilation_unit->source_code,&match_source_file);

        for(line_number = nearest_source_record->line_number - LIBVALVE_SHARED_MEM->config.context_num_lines; line_number <= nearest_source_record->line_number + LIBVALVE_SHARED_MEM->config.context_num_lines; line_number++)
        {
            if(line_number > 0 && line_number <= source_code->num_lines)
            {
              if(line_number == nearest_source_record->line_number)
                fprintf(stderr,"-> %d: %s\n",line_number,source_code->source_code[line_number - 1]);
              else
                fprintf(stderr,"   %d: %s\n",line_number,source_code->source_code[line_number - 1]);
            }
        }
        
        fprintf(stderr,"\n");
        
      }
    }
  }

  if(num_leaks == 0)
  {
    fprintf(stderr,"[libvalve] No leaks detected.\n");
  }
}
