#include <stdio.h>  
#include <string.h> 

struct A {
  int a;
  int b;
};

struct A s_add(struct A a, struct A b)
{
  b.a = b.a + a.a;
  b.b = b.b + a.b;
  return b;
}

int add(int x, int y, int z)
{
  int a;
  a = x + y + z;
  return a;
}

int foo(int x, int y, int z)
{
  int a = 0;
  x += 1;
  a = add(x, y, z);
  a = a + 1;
  return a;
}

int main(int argc, char** argv)
{
  int x = 1;
  int y = 2;
  int z = 3;
  foo(x, y, z);
  struct A a = {1,2};
  struct A b = {2,3};
  struct A c;
  c = s_add(a, b);
  return 0;
}
