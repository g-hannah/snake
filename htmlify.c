#include <errno.h>
#include <fcntl.h>
#include <misclib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define IBUF_SIZE	4096
#define OBUF_SIZE	16384

#define PREF_HEAD		"<h1>"
#define PREF_HEAD_LEN		4
#define PREF_HEAD_END		"</h1>"
#define PREF_HEAD_END_LEN	5

static int		fd, ofd;
static struct stat	statb;
static char		*IBUF = NULL, *OBUF = NULL, *TMP = NULL;

int create_page(char *fname, char *out) __nonnull ((1,2)) __wur;

int
main(int argc, char *argv[])
{
	if (argc < 2)
	  { fprintf(stderr, "htmlify <file> <outfile>\n"); exit(0xff); }
	else if (argc < 3)
	  { fprintf(stderr, "htmlify <file> <outfile>\n"); exit(0xff); }

	if (create_page(argv[1], argv[2]) < 0)
	  { fprintf(stderr, "main() > create_page()"); }

	exit(0);
}

int
create_page(char *fname, char *out)
{
	static char		*p = NULL, *q = NULL, *a = NULL;
	static size_t		tsize;
	static ssize_t		n;
	static int		i, nlcnt;

	if (access(fname, F_OK) != 0)
	  { fprintf(stderr, "create_page(): file does not exist\n"); return(-1); }

	if (access(fname, R_OK) != 0)
	  { fprintf(stderr, "create_page(): cannot read this file\n"); return(-1); }

	memset(&statb, 0, sizeof(statb));

	if (lstat(fname, &statb) < 0)
	  { fprintf(stderr, "create_page() > lstat(): %s\n", strerror(errno)); return(-1); }

	if (!(IBUF = malloc(statb.st_size+1)))
	  { fprintf(stderr, "create_page() > malloc(): %s\n", strerror(errno)); return(-1); }

	if (!(OBUF = malloc((statb.st_size*3))))
	  { fprintf(stderr, "create_page() > malloc(): %s\n", strerror(errno)); return(-1); }

	if ((fd = open(fname, O_RDONLY)) < 0)
	  { fprintf(stderr, "create_page() > open(): %s\n", strerror(errno)); return(-1); }

	if ((ofd = open(out, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU & ~S_IXUSR)) < 0)
	  { fprintf(stderr ,"create_page() > open(): %s\n", strerror(errno)); return(-1); }
	
	tsize = statb.st_size;
	i &= ~i;
	sprintf(OBUF, "<p>");

	n = read_n(fd, IBUF, statb.st_size);
	q = (OBUF + 3);
	p = IBUF;

	while (tsize > 0)
	  {
		nlcnt &= ~nlcnt;
		while (p < (IBUF + statb.st_size))
		  {
			if (*p == 0x22)
			  {
			  	sprintf(q, "&quot;");
				q+=6;
				++p;
				if (p == (IBUF + n))
					break;
			  }
			else if (*p == 0x0a || *p == 0x0d)
			  {
				if ((*p == 0x0d && *(p+2) == 0x0d) || (*p == 0x0a && *(p+1) == 0x0a)) // new para
				  {
					if (nlcnt == 0) // isolated line, use <h1> tags
					  {
						a = q;

						while (1)
						  {
							while (*a != 0x3c && a > (OBUF + 1))
								--a;
							if (strncasecmp("<p>", a, 3) != 0)
							  { --a; continue; }
							else
							  { break; }
						  }

						a += 3;
						if (!(TMP = malloc((q - a)+1)))
							return(-1);

						strncpy(TMP, a, (q - a));
						TMP[(q - a)] = 0;
						a -= 3;

						strcpy(a, PREF_HEAD);
						//sprintf(a, "%s", PREF_HEAD);
						a += PREF_HEAD_LEN;

						strcpy(a, TMP);
						//snprintf(a, strlen(TMP), "%s", TMP);
						a += strlen(TMP);
						free(TMP);
						q = a;

						sprintf(q, "%s\n<p>",PREF_HEAD_END);
						//sprintf(q, "%s\n", PREF_HEAD_END);

						q += (PREF_HEAD_END_LEN + 4);

						while ((*p == 0x0a || *p == 0x0d) && p < (IBUF + statb.st_size))
							++p;
						nlcnt &= ~nlcnt;
						if (p >= (IBUF + statb.st_size)) // not IBUF_SIZE
							break;
					  }
					else // nlcnt != 0
					  {
						sprintf(q, "</p>\n<p>");
						q += 8;
						while ((*p == 0x0a || *p == 0x0d) && p < (IBUF + statb.st_size))
							++p;
						nlcnt &= ~nlcnt;
						if (p >= (IBUF + statb.st_size))
							break;
							
					  }
				  }
				else
				// next char not 0x0a / *(p+2) not a 0x0d
				  {
					if (*p == 0x0d)
						++p;
					if (p >= (IBUF + statb.st_size))
						break;

					*q++ = *p++;
					++nlcnt;
					if (p >= (IBUF + statb.st_size))
						break;
				  }
			  }
			else if (*p == 0x27)
			  {
				sprintf(q, "&apos;");
				q+=6;
				++p;
				if (p >= (IBUF + statb.st_size))
					break;
			  }
			else if (*p == 0x26)
			  {
				sprintf(q, "&amp;");
				q+=5;
				++p;
				if (p >= (IBUF + statb.st_size))
					break;
			  }
			else if (*p == 0x3c)
			  {
				sprintf(q, "&lt;");
				q+=4;
				++p;
				if (p >= (IBUF + statb.st_size))
					break;
			  }
			else if (*p == 0x3d)
			  {
				sprintf(q, "&#61;");
				q+=5;
				++p;
				if (p >= (IBUF + statb.st_size))
					break;
			  }
			else if (*p == 0x3e)
			  {
				sprintf(q, "&gt;");
				q+=4;
				++p;
				if (p >= (IBUF + statb.st_size))
					break;
			  }
			else
			  {
				*q++ = *p++;
				if (p >= (IBUF + statb.st_size))
					break;
			  }
		  }

		*q = 0;
		write_n(ofd, OBUF, (q - OBUF));
		tsize -= n;
	  }

	sync();
	close(ofd);
	free(IBUF);
	free(OBUF);
	return(0);
}
