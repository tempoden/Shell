
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "shell.h"

int promptline(char *prompt_, char *line, int sizline)
{
	int n = 0;
	char prompt[1024];
	sprintf(prompt,"\x1b[94mMYSHELL:\x1b[92m[%s]\x1b[0m ", prompt_);
	write(1, prompt, strlen(prompt));
	while (1)
	{
		n += read(0, (line + n), sizline-n);
		*(line+n) = '\0';
		/*
		*  check to see if command line extends onto
		*  next line.  If so, append next line to command line
		*/

		if ((*(line+n-2) == '\\') && (*(line+n-1) == '\n'))
		{
			*(line+n) = ' ';
			*(line+n-1) = ' ';
			*(line+n-2) = ' ';
			continue;   /*  read next line  */
		}
		return(n);      /* all done */
	}
}
