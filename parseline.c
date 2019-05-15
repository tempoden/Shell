#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "shell.h"

static char *blankskip(char *);

int parseline(char *line)
{
	int nargs, ncmds;
	register char *s;
	char aflg = 0;
	int rval;
	register int i;
	static char delim[] = " \t|&<>;\n";

	/* initialize  */
	//bkgrnd=0;
	nargs=0;
	ncmds=0;
	rval=0;
	s=line;
	//infile=NULL;
	//outfile=NULL;
	//appfile=NULL;
	cmds[0].cmdargs[0]=NULL;
	for (i=0;i < MAXCMDS;i++)
	{
		cmds[i].cmdflag=0;
		cmds[i].bkgrnd=0;
		cmds[i].infile=NULL;
		cmds[i].outfile=NULL;
		cmds[i].appfile=NULL;
	}

	while (*s)
	{
		/* until line has been parsed */
		s=blankskip(s);       /*  skip white space */
		if(!*s)
		{
			break; /*  done with line */
		}

		/*  handle <, >, |, &, and ;  */
		switch(*s)
		{
			case '&':
				++cmds[ncmds].bkgrnd;
				*s++ = '\0';
				break;
			case '>':
				if (*(s+1) == '>')
				{
					++aflg;
					*s++ = '\0';
				}
				*s++ = '\0';
				s = blankskip(s);
				if (!*s)
				{
					fprintf(stderr, "syntax error\n");
					return(-1);
				}

				if (aflg)
				{
					cmds[ncmds].appfile = s;
				}
				else
				{
					cmds[ncmds].outfile = s;
				}
				s = strpbrk(s, delim);
				if (isspace(*s))
				{
					*s++ = '\0';
				}
				break;
			case '<':
				*s++ = '\0';
				s = blankskip(s);
				if (!*s)
				{
					fprintf(stderr, "syntax error\n");
					return(-1);
				}
				cmds[ncmds].infile = s;
				s = strpbrk(s, delim);
				if(isspace(*s))
				{
					*s++ = '\0';
				}
				break;
			case '|':
				if (nargs == 0)
				{
					fprintf(stderr, "syntax error\n");
					return(-1);
				}
				cmds[ncmds++].cmdflag |= OUTPIP;
				cmds[ncmds].cmdflag |= INPIP;
				*s++ = '\0';
				nargs = 0;
				break;
			case ';':
				*s++ = '\0';
				++ncmds;
				nargs = 0;
				break;
			default:
				/*  a command argument  */
				if (nargs == 0) /* next command */
				{
					rval = ncmds+1;
				}
				cmds[ncmds].cmdargs[nargs++] = s;
				cmds[ncmds].cmdargs[nargs] = (char *) NULL;
				s = strpbrk(s, delim);
				if (isspace(*s))
				{
					*s++ = '\0';
				}
				break;
		}  /*  close switch  */
	}  /* close while  */

	/*  error check  */

	/*
	*  The only errors that will be checked for are
	*  no command on the right side of a pipe
	*  no command to the left of a pipe is checked above
	*/
	if (cmds[ncmds-1].cmdflag & OUTPIP)
	{
		if (nargs == 0)
		{
			fprintf(stderr, "syntax error\n");
			return(-1);
		}
	}

	return(rval);
}

static char *blankskip(char *s)
{
	while (isspace(*s) && *s)
	{
		++s;
	}
	return(s);
}
