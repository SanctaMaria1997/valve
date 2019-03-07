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
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ptrace.h>
#include <elf.h>
#include <dwarf.h>
#include <getopt.h>
#include "elf_util.h"
#include "valve_util.h"
#include "valve.h"
#include "dwarfy.h"

#ifdef FREEBSD
#define PTRACE_TRACEME PT_TRACE_ME
#define PTRACE_CONT PT_CONTINUE
#define PTRACE_GETREGS PT_GETREGS
#define PTRACE_PEEKDATA PT_READ_D
#define PTRACE_POKEDATA PT_WRITE_D
#define PTRACE_VM_ENTRY PT_VM_ENTRY
#else
#define r_
#endif

extern char **environ;
LibvalveSharedMem *LIBVALVE_SHARED_MEM;
char LIBVALVE_PATCHED_LIB_NAMES[LIBVALVE_MAX_NUM_LIBRARIES][256];
int LIBVALVE_NUM_PATCHED_LIBS;
Library *lookup_library(char *name);

void patch_function(pid_t pid,unsigned long int orig,unsigned long int wrapper)
{
  int high,low;
  low = (int)(wrapper & 0xFFFFFFFF);
  high = (int)((wrapper & 0xFFFFFFFF00000000) >> 32);
  ptrace(PTRACE_POKEDATA,pid,(caddr_t)orig,low);
  ptrace(PTRACE_POKEDATA,pid,(caddr_t)(orig + 4),high);
}

void patch_mem_functions(pid_t pid,Library *destination,Library *source)
{
  unsigned long int relocation;
  
  if((relocation = get_elf_relocation(destination->elf,"malloc")))
  {
    patch_function(pid,
                   relocation - get_elf_base_address(destination->elf) + destination->base_address,
                   get_elf_symbol(source->elf,"malloc_wrapper") - get_elf_base_address(source->elf) + source->base_address);
  } 
  if((relocation = get_elf_relocation(destination->elf,"calloc")))
  {
    patch_function(pid,
                   relocation - get_elf_base_address(destination->elf) + destination->base_address,
                   get_elf_symbol(source->elf,"calloc_wrapper") - get_elf_base_address(source->elf) + source->base_address);
  }
  if((relocation = get_elf_relocation(destination->elf,"realloc")))
  {
    patch_function(pid,
                   relocation - get_elf_base_address(destination->elf) + destination->base_address,
                   get_elf_symbol(source->elf,"realloc_wrapper") - get_elf_base_address(source->elf) + source->base_address);
  }
  if((relocation = get_elf_relocation(destination->elf,"free")))
  {
    patch_function(pid,
                   relocation - get_elf_base_address(destination->elf) + destination->base_address,
                   get_elf_symbol(source->elf,"free_wrapper") - get_elf_base_address(source->elf) + source->base_address);
  }
}

void load_mem_regions(pid_t pid)
{
  struct ptrace_vm_entry entry;
  unsigned long int prev_start = 0;
  char *mmm = malloc(256);
  entry.pve_entry = 0;
  entry.pve_start = 0;
  entry.pve_path = mmm;
  int i,j,k;
  i = 0;
  j = 0;
  k = 0;
  
  do
  {
    prev_start = entry.pve_start;
    entry.pve_pathlen = 256;
    ptrace(PTRACE_VM_ENTRY,pid,(caddr_t)(&entry),0);

    for(j = 0; j < k; j++)
    {
      if(!strcmp(file_part(mmm),file_part(LIBVALVE_SHARED_MEM->libraries[j].name)))
        goto skip;
    }
    
    LIBVALVE_SHARED_MEM->libraries[k].base_address = (unsigned long int)(entry.pve_start);
    strcpy(LIBVALVE_SHARED_MEM->libraries[k].name,file_part(mmm));
    
    k++;
    skip:
    i++;

  } while(entry.pve_start != prev_start); 

}

int main(int argc,char **argv)
{
  pid_t pid;
  int result = 1;
  int status;
  char target_name[512];
  int shmid;
  Library *target,*libvalve;
  int opt = 0;
  int i,j;
  
  shmid = shmget(ftok("/usr/local/lib/libvalve.so",1),sizeof(LibvalveSharedMem),IPC_CREAT | 0666);
  LIBVALVE_SHARED_MEM = shmat(shmid,0,0);
 
  memset(LIBVALVE_SHARED_MEM,0,sizeof(LibvalveSharedMem));
  
  LIBVALVE_NUM_PATCHED_LIBS = 0;
  LIBVALVE_SHARED_MEM->config.context_num_lines = 1;
  
  while((opt = getopt(argc,argv,":p:c:")) != -1)
  {
      switch(opt)
      {
        case 'p':
        {
          strcpy(LIBVALVE_PATCHED_LIB_NAMES[LIBVALVE_NUM_PATCHED_LIBS],optarg);
          LIBVALVE_NUM_PATCHED_LIBS++;
          break;
        }
        case 'c':
        {
          int num_lines;
          sscanf(optarg,"%d",&num_lines);
          LIBVALVE_SHARED_MEM->config.context_num_lines = num_lines;
          break;
        }
        case ':':
        {
          exit(1);
          break;
        }  
        case '?':
        {
          break;
        }
      }
      
  }
  
  strcpy(target_name,*(argv + optind));
  
  switch(pid = fork())
  {
      case -1:
      {
          fprintf(stderr,"Unable to launch traced process; exiting.\n");
          exit(1);
      }
      case 0:
      {
        ptrace(PTRACE_TRACEME,0,0,0);
        putenv("LD_PRELOAD=/usr/local/lib/libvalve.so");
        exect(target_name,argv + optind,environ);
        break;
      }
      default:
      {
        
        waitpid(pid,&status,0);
        ptrace(PTRACE_CONT,pid,(caddr_t)1,0);
        waitpid(pid,&status,0);
        
        load_mem_regions(pid);

        libvalve = lookup_library("libvalve.so");
        target = lookup_library(target_name);
        target->elf = load_elf(find_file(file_part(target_name),"."));
        libvalve->elf = load_elf(find_file("libvalve.so","/usr/local/lib/"));
       
        patch_mem_functions(pid,target,libvalve);
        
        for(i = 0; i < LIBVALVE_NUM_PATCHED_LIBS; i++)
        {
            j = 0;
            while(strlen(LIBVALVE_SHARED_MEM->libraries[j].name))
            {
              if(!strcmp(LIBVALVE_PATCHED_LIB_NAMES[i],LIBVALVE_SHARED_MEM->libraries[j].name))
              {
                LIBVALVE_SHARED_MEM->libraries[j].elf = load_elf(find_file(LIBVALVE_SHARED_MEM->libraries[j].name,"."));
                if(LIBVALVE_SHARED_MEM->libraries[j].elf == 0)
                {
                  fprintf(stderr,"[libvalve] Error: unable to locate shared object \"%s\" in current directory hierarchy.\n",LIBVALVE_SHARED_MEM->libraries[j].name);
                  exit(1);
                }
                
                fprintf(stderr,"[libvalve] Patching \"%s\"...\n",LIBVALVE_PATCHED_LIB_NAMES[i]);
  
                patch_mem_functions(pid,&LIBVALVE_SHARED_MEM->libraries[j],libvalve);
              }
              j++;
            }          
        }
        
        ptrace(PTRACE_CONT,pid,(caddr_t)1,0);
        
        for(;;)
        {
          waitpid(pid,&status,0);
          
          if(WIFSTOPPED(status))
          {
            ptrace(PTRACE_CONT,pid,(caddr_t)1,0);  
          }
          else if(WIFEXITED(status))
          {
            result = WEXITSTATUS(status);
            break;
          }
       }
     }
  }
  
  fprintf(stderr,"[libvalve] Program exited with code %d.\n",result);
  return 0;
}

Library *lookup_library(char *name)
{
  int i = 0;

  while(strlen(LIBVALVE_SHARED_MEM->libraries[i].name))
  {
    if(!strcmp(file_part(LIBVALVE_SHARED_MEM->libraries[i].name),file_part(name)))
      return &LIBVALVE_SHARED_MEM->libraries[i];
    i++;
  }
  return 0;
}
        
