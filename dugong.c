#include <stdio.h>
#include <stdlib.h>

void dugong()
{
  void *p;
  p = malloc(123);
  puts("Hi from dugong!");
  p = malloc(321);
}
