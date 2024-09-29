#include <stdio.h>
#include "getline/getline.h"

int main()
/* 
 * just echo user input lines, letting user edit them and move through
 * history list
 */
{
    char *p;

    do {
	p = gl_getline("PROMPT>>>> ");
	gl_histadd(p);
	fputs(p, stdout);
    } while (*p != 0);
    return 0;
}
