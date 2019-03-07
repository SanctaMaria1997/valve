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

#include <stdlib.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>

char *REQUESTED_FILE_NAME;
char *MATCHING_FILE_PATH;
int FILE_PATH_EXISTS;

char *file_part(char *path)
{
  int last_slash = 0;
  int i;
  for(i = 0; i < strlen(path); i++)
  {
    if(path[i] == '/')
    {
      last_slash = i;
    }
  }
  return path + last_slash + (last_slash ? 1 : 0);
}

int evaluate_item(const char *path, const struct stat *info,const int typeflag, struct FTW *path_info)
{
    if (typeflag == FTW_F)
    {
      if(!strcmp(path + path_info->base,REQUESTED_FILE_NAME))
      {
        MATCHING_FILE_PATH = strdup(path);
        FILE_PATH_EXISTS = 1;
        return 1;
      }
    }
    return 0;
}


char *find_file(char *file_name,char *directory)
{
  MATCHING_FILE_PATH = 0;
  REQUESTED_FILE_NAME = file_name;
  FILE_PATH_EXISTS = 0;
  nftw(directory,evaluate_item,15,FTW_PHYS);
  if(FILE_PATH_EXISTS)
    return MATCHING_FILE_PATH;
  else
    return 0;
}
