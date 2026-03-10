char *rcs_lex = "$Id: lex.c,v 2.1 1994/04/15 19:00:28 celes Exp $";
/*$Log: lex.c,v $
 * Revision 2.1  1994/04/15  19:00:28  celes
 * Retirar chamada da funcao lua_findsymbol associada a cada
 * token NAME. A decisao de chamar lua_findsymbol ou lua_findconstant
 * fica a cargo do modulo "lua.stx".
 *
 * Revision 1.3  1993/12/28  16:42:29  roberto
 * "include"s de string.h e stdlib.h para evitar warnings
 *
 * Revision 1.2  1993/12/22  21:39:15  celes
 * Tratamento do token $debug e $nodebug
 *
 * Revision 1.1  1993/12/22  21:15:16  roberto
 * Initial revision
 **/

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "opcode.h"
#include "hash.h"
#include "inout.h"
#include "table.h"
#include "y.tab.h"

#define next() { current = input(); }
#define save(x) { *yytextLast++ = (x); }
#define save_and_next()  { save(current); next(); }

static int current;
static char yytext[256];
static char *yytextLast;

static Input input;

void lua_setinput (Input fn)
{
  current = ' ';
  input = fn;
}

char *lua_lasttext (void)
{
  *yytextLast = 0;
  return yytext;
}


static struct 
  {
    char *name;
    int token;
  } reserved [] = {
      {"and", AND},
      {"do", DO},
      {"else", ELSE},
      {"elseif", ELSEIF},
      {"end", END},
      {"function", FUNCTION},
      {"if", IF},
      {"local", LOCAL},
      {"nil", NIL},
      {"not", NOT},
      {"or", OR},
      {"repeat", REPEAT},
      {"return", RETURN},
      {"then", THEN},
      {"until", UNTIL},
      {"while", WHILE} };

#define RESERVEDSIZE (sizeof(reserved)/sizeof(reserved[0]))


int findReserved (char *name)
{
  int l = 0;
  int h = RESERVEDSIZE - 1;
  while (l <= h)
  {
    int m = (l+h)/2;
    int comp = strcmp(name, reserved[m].name);
    if (comp < 0)
      h = m-1;
    else if (comp == 0)
      return reserved[m].token;
    else
      l = m+1;
  }
  return 0;
}

/*
【我自己的理解：yylex 到底在干嘛？】

这函数就是 Lua 1.1 的“切词器/分词器”。
它不是去理解语法，也不是去执行代码，它只干一件事：
从输入里一个字符一个字符读，然后“切成 token”交给 yyparse（语法分析器）。

yyparse 不会直接看原始字符，它只认 token：
比如 NAME、NUMBER、STRING、IF、WHILE、'+'、'-' 这种。
所以 yylex 的工作做不好，后面语法分析就直接崩，或者会出现“明明看着对但解析报错”的情况。

我一开始读源码最容易迷糊的点是：yylex 自己要负责跳过空白/注释，
因为“空格”和“注释”对语法来说就是不存在的，不该变成 token。
*/
int yylex ()
{
/* 
这里 while(1) 的意思是：yylex 会一直读，直到“凑出一个 token”才 return。
如果读到的是空格/注释这种“没用的东西”，就 continue 继续读下一段。
所以 yylex 不是一次读完文件，它是一边被 yyparse 调用、一边按需产出 token。
*/  
while (1)
  {
    yytextLast = yytext;
    switch (current)
    {
/*
空白处理（很关键）：
- 空格/Tab：直接跳过，不输出 token
- 换行：也跳过，但要顺便把 lua_linenumber++，这样报错才能指出正确行号
这里用的是“case '\n' 不 break，继续落到空格/Tab 的逻辑”，属于一种省事写法。
*/
      case '\n': lua_linenumber++;
      case ' ':
      case '\t':
        next();
        continue;

      case '$':
	next();
	while (isalnum(current) || current == '_')
          save_and_next();
        *yytextLast = 0;
	if (strcmp(yytext, "debug") == 0)
	{
	  yylval.vInt = 1;
	  return DEBUG;
        }
	else if (strcmp(yytext, "nodebug") == 0)
	{
	  yylval.vInt = 0;
	  return DEBUG;
        }
	return WRONGTOKEN;
  
  /*
【Hack 点：注释符号】

原版 Lua 1.1 的习惯是用 “--” 做单行注释：看到两个 '-' 就把这一行吃掉。
我这里为了做 Hack，额外支持了 “//” 这种注释（更像 C/Java 的习惯）：
- 如果遇到 '/'，再看下一个是不是也是 '/'
- 是的话就一直 next() 吃到行尾（'\n' 或 EOF），然后 continue
- 注意：continue 的意思就是“注释不生成 token”，对语法分析来说就当没看见

我这里还有一个踩坑点：
我现在的 case '-' 被我改成了直接返回 '-'，所以原版的 “-- 注释”其实被我破坏了。
如果老师要求保留原本语法，那我应该把 “--” 注释逻辑也加回来：
遇到 '-' 时，如果下一个还是 '-'，就吃到行尾；否则才返回 '-'。
这一点我会在报告里写成 Bug 复盘（属于我真正读代码后才意识到的坑）。
*/   
case '-':
  save_and_next();
  return '-';
case '/':
  save_and_next();
  if (current != '/') return '/';
  do { next(); } while (current != '\n' && current != 0);
  continue;
  
      case '<':
        save_and_next();
        if (current != '=') return '<';
        else { save_and_next(); return LE; }
  
      case '>':
        save_and_next();
        if (current != '=') return '>';
        else { save_and_next(); return GE; }
  
      case '~':
        save_and_next();
        if (current != '=') return '~';
        else { save_and_next(); return NE; }

/*
字符串处理：
这里支持 "..." 和 '...' 两种字符串。
逻辑很直白：先记住用的是什么引号(del)，然后一直读到同一个引号结束。

中途要处理转义：
比如 '\n'、'\t'、'\r' 这些要转换成真正的字符。
如果读到换行或 EOF 还没闭合，就当成坏 token（WRONGTOKEN），交给上层报错。
最后把字符串放进常量表 lua_findconstant，并返回 STRING token。
*/
      case '"':
      case '\'':
      {
        int del = current;
        next();  /* skip the delimiter */
        while (current != del) 
        {
          switch (current)
          {
            case 0: 
            case '\n': 
              return WRONGTOKEN;
            case '\\':
              next();  /* do not save the '\' */
              switch (current)
              {
                case 'n': save('\n'); next(); break;
                case 't': save('\t'); next(); break;
                case 'r': save('\r'); next(); break;
                default : save('\\'); break;
              }
              break;
            default: 
              save_and_next();
          }
        }
        next();  /* skip the delimiter */
        *yytextLast = 0;
        yylval.vWord = lua_findconstant (yytext);
        return STRING;
      }
/*
NAME / 关键字处理：
我把它理解成“先把一个单词完整读出来，再决定它是什么”。

- 读规则：字母或 '_' 开头，后面可以跟字母/数字/'_'
- 读完后先去 reserved[] 里查是不是关键字（and/if/while 等）
  - 是关键字：直接返回关键字 token
  - 不是：返回 NAME，并把文本放到 yylval 里，给语法分析器用

这里用二分查找 findReserved()，主要是因为关键字表是排序好的，查起来快一点。
*/
      case 'a': case 'b': case 'c': case 'd': case 'e':
      case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o':
      case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y':
      case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E':
      case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O':
      case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y':
      case 'Z':
      case '_':
      {
        int res;
        do { save_and_next(); } while (isalnum(current) || current == '_');
        *yytextLast = 0;
        res = findReserved(yytext);
        if (res) return res;
        yylval.pChar = yytext;
        return NAME;
      }
   
      case '.':
        save_and_next();
        if (current == '.') 
        { 
          save_and_next(); 
          return CONC;
        }
        else if (!isdigit(current)) return '.';
        /* current is a digit: goes through to number */
        goto fraction;

      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      
        do { save_and_next(); } while (isdigit(current));
        if (current == '.') save_and_next();
fraction: while (isdigit(current)) save_and_next();
        if (current == 'e' || current == 'E')
        {
          save_and_next();
          if (current == '+' || current == '-') save_and_next();
          if (!isdigit(current)) return WRONGTOKEN;
          do { save_and_next(); } while (isdigit(current));
        }
        *yytextLast = 0;
        yylval.vFloat = atof(yytext);
        return NUMBER;

      default: 		/* also end of file */
      {
        save_and_next();
        return *yytext;      
      }
    }
  }
}
        
