/*
** inout.c
** Provide function to realise the input/output function and debugger 
** facilities.
*/

/*
【我自己的理解：inout.c 是干嘛的？】

这个文件我把它当成“输入适配层”。
Lua 的词法分析器 yylex() 并不关心你是从文件读，还是从字符串读，
它只会不停地要“下一个字符”。

所以这里做了一件很聪明但也很容易忽略的事：
- 用 lua_setinput(...) 把“读字符的函数”换成 fileinput 或 stringinput
- 后面 yylex 只要调用 input() 就行，不需要写两套逻辑

也就是说：inout.c 负责把输入源统一成一种“按字符读取”的接口。
*/
char *rcs_inout="$Id: inout.c,v 1.2 1993/12/22 21:15:16 roberto Exp $";

#include <stdio.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"

/* Exported variables */
int lua_linenumber;
int lua_debug;
int lua_debugline;

/* Internal variables */
#ifndef MAXFUNCSTACK
#define MAXFUNCSTACK 32
#endif
static struct { int file; int function; } funcstack[MAXFUNCSTACK];
static int nfuncstack=0;

static FILE *fp;
static char *st;
static void (*usererror) (char *s);

/*
** Function to set user function to handle errors.
*/
void lua_errorfunction (void (*fn) (char *s))
{
 usererror = fn;
}

/*
** Function to get the next character from the input file
*/

/*
fileinput：从文件里读一个字符。
这里把 EOF 转成 0（不是返回 EOF），我理解是为了让后续代码统一用 0 表示“读完了/没字符了”。
这样 yylex 里判断结束条件就更简单，不用到处处理 EOF。
*/
static int fileinput (void)
{
 int c = fgetc (fp);
 return (c == EOF ? 0 : c);
}

/*
** Function to get the next character from the input string
*/

/*
stringinput：从字符串里读一个字符。
st 是指向当前读到哪里的指针，每次读一个字符就 st++ 往后走一步。

这里写法是先 st++，再返回 *(st-1)。
我理解成：返回的是“刚刚那一格的字符”，同时把指针推进到下一格。
*/
static int stringinput (void)
{
 st++;
 return (*(st-1));
}

/*
** Function to open a file to be input unit. 
** Return 0 on success or 1 on error.
*/

/*
lua_openfile：把输入源切换成“文件模式”。

我把它理解成：这一步不是解析脚本，而是做准备工作：
1) lua_linenumber = 1：从第一行开始计数（报错才有行号）
2) lua_setinput(fileinput)：告诉词法分析器：之后读字符就用 fileinput
3) fopen(fn, "r")：真正打开文件
4) lua_addfile(fn)：把文件名登记起来，出错时能打印 “在哪个文件第几行”

所以这函数的核心就是：让后面 yylex() 能“从这个文件里”连续读字符。
*/
int lua_openfile (char *fn)
{
 lua_linenumber = 1;
 lua_setinput (fileinput);
 fp = fopen (fn, "r"); /* fp 是全局静态变量，后面 fileinput 就靠它 fgetc */
 if (fp == NULL) return 1;
 if (lua_addfile (fn)) return 1;
 return 0;
}

/*
** Function to close an opened file
*/
void lua_closefile (void)/*
lua_closefile：文件模式的收尾。

这里我觉得最重要的是 “fp = NULL”：
因为 fp 是静态全局变量，如果不清掉，下一次 openfile/或者报错处理可能会误以为文件还开着。
lua_delfile() 也很关键：把“当前文件”从登记表里删掉，不然报错信息会串。
*/
{
 if (fp != NULL)
 {
  lua_delfile();
  fclose (fp);
  fp = NULL;
 }
}

/*
** Function to open a string to be input unit
*/

/*
lua_openstring：把输入源切换成“字符串模式”。

跟 openfile 一样，重点是：
- lua_setinput(stringinput)：之后读字符用 stringinput
- st = s：把字符串指针保存下来，stringinput 才知道从哪读

这里还有个小细节我觉得挺有意思：
它会构造一个假的“文件名”叫 "String: xxxx..."
然后 lua_addfile(sn) 登记进去。
这样如果字符串执行出错，报错信息里也会显示一个“来源”，不至于完全没头绪。
*/
int lua_openstring (char *s)
{
 lua_linenumber = 1;
 lua_setinput (stringinput);
 st = s;
 {
  char sn[64];
  sprintf (sn, "String: %10.10s...", s);
  if (lua_addfile (sn)) return 1;
 }
 return 0;
}

/*
** Function to close an opened string
*/

/*
lua_closestring：字符串模式的收尾。

虽然字符串不用 fclose，但它也要 lua_delfile()：
原因很简单：错误信息或者调试信息依赖当前输入源是谁，不删就会把下一次执行的来源搞混。
*/
void lua_closestring (void)
{
 lua_delfile();
}

/*
** Call user function to handle error messages, if registred. Or report error
** using standard function (fprintf).
*/
void lua_error (char *s)
/*
lua_reportbug：这个函数就是“把报错信息说得更像人话”。

我自己的理解：
- s 是错误描述的开头
- 如果 lua_debugline != 0，说明 VM 记录了“出错语句大概在第几行”
- 如果 nfuncstack > 0，说明在函数调用里面出错了，就把调用栈一路打印出来（类似很简化的 stack trace）
- 最后把拼好的 msg 交给 lua_error 输出

这块代码让我更确定 Lua 1.1 虽然老，但已经有“报错定位 + 调用栈”的雏形了。
*/
{
 if (usererror != NULL) usererror (s);
 else			    fprintf (stderr, "lua: %s\n", s);
}

/*
** Called to execute  SETFUNCTION opcode, this function pushs a function into
** function stack. Return 0 on success or 1 on error.
*/
int lua_pushfunction (int file, int function)
{
 if (nfuncstack >= MAXFUNCSTACK-1)
 {
  lua_error ("function stack overflow");
  return 1;
 }
 funcstack[nfuncstack].file = file;
 funcstack[nfuncstack].function = function;
 nfuncstack++;
 return 0;
}

/*
** Called to execute  RESET opcode, this function pops a function from 
** function stack.
*/
void lua_popfunction (void)
{
 nfuncstack--;
}

/*
** Report bug building a message and sending it to lua_error function.
*/
void lua_reportbug (char *s)
{
 char msg[1024];
 snprintf(msg, sizeof(msg), "%s", s);
 if (lua_debugline != 0)
 {
  int i;
  if (nfuncstack > 0)
  {
   sprintf (strchr(msg,0), 
         "\n\tin statement begining at line %d in function \"%s\" of file \"%s\"",
         lua_debugline, s_name(funcstack[nfuncstack-1].function),
  	 lua_file[funcstack[nfuncstack-1].file]);
   sprintf (strchr(msg,0), "\n\tactive stack\n");
   for (i=nfuncstack-1; i>=0; i--)
    sprintf (strchr(msg,0), "\t-> function \"%s\" of file \"%s\"\n", 
                            s_name(funcstack[i].function),
			    lua_file[funcstack[i].file]);
  }
  else
  {
   sprintf (strchr(msg,0),
         "\n\tin statement begining at line %d of file \"%s\"", 
         lua_debugline, lua_filename());
  }
 }
 lua_error (msg);
}

