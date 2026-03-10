/*
** lua.c
** Linguagem para Usuarios de Aplicacao
*/

char *rcs_lua="$Id: lua.c,v 1.1 1993/12/17 18:41:19 celes Exp $";

#include <stdio.h>
#include <string.h>
#include "lua.h"
#include "lualib.h"

void main (int argc, char *argv[])
{
 int i;
 iolib_open ();
 strlib_open ();
 mathlib_open ();
 if (argc < 2)
 {
   char buffer[2048];
      while (fgets(buffer, sizeof(buffer), stdin) != NULL)
   {
     /* 去掉 fgets 读到的行尾换行符，避免影响解析 */
     size_t len = strlen(buffer);
     if (len > 0 && buffer[len-1] == '\n') buffer[len-1] = '\0';

     lua_dostring(buffer);
   }
 }
 else
   for (i=1; i<argc; i++)
    lua_dofile (argv[i]);
}
