#include <errno.h>
#include <fcntl.h>
#include <misclib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int		fd, ofd;
static struct stat	statb;
static char		*IBUF = NULL, *OBUF = NULL;

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
	static char		*p = NULL, *q = NULL, c;
	static size_t		tsize;
	static ssize_t		n;
	static int		i;

	if (access(fname, F_OK) != 0)
	  { fprintf(stderr, "create_page(): file does not exist\n"); return(-1); }

	if (access(fname, R_OK) != 0)
	  { fprintf(stderr, "create_page(): cannot read this file\n"); return(-1); }

	memset(&statb, 0, sizeof(statb));

	if (!(IBUF = (char *)calloc(4096, sizeof(char))))
	  { fprintf(stderr, "create_page() > calloc(): %s\n", strerror(errno)); return(-1); }

	if (!(OBUF = (char *)calloc(16384, sizeof(char))))
	  { fprintf(stderr, "create_page() > calloc(): %s\n", strerror(errno)); return(-1); }

	if (lstat(fname, &statb) < 0)
	  { fprintf(stderr, "create_page() > lstat(): %s\n", strerror(errno)); return(-1); }

	if ((fd = open(fname, O_RDONLY)) < 0)
	  { fprintf(stderr, "create_page() > open(): %s\n", strerror(errno)); return(-1); }

	if ((ofd = open(out, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU & ~S_IXUSR)) < 0)
	  { fprintf(stderr ,"create_page() > open(): %s\n", strerror(errno)); return(-1); }
	
	tsize = statb.st_size;
	i &= ~i;

	while (tsize > 0)
	  {
		if ((n = read_n(fd, IBUF, 4095)) == -1)
			return(-1);
		IBUF[n] = 0;
		p = IBUF;
		q = OBUF;

		while (p < (IBUF + n))
		  {
			if (*p == 0x22)
			  { sprintf(q, "&quot;"); q+=6; ++p; }
			else if (*p == 0x0a)
			  { sprintf(q, "<br>\n"); q+=5; ++p; }
			else if (*p == 0x27)
			  { sprintf(q, "&apos;"); q+=6; ++p; }
			else if (*p == 0x26)
			  { sprintf(q, "&amp;"); q+=4; ++p; }
			else if (*p == 0x3c)
			  { sprintf(q, "&lt;"); q+=4; ++p; }
			else if (*p == 0x3e)
			  { sprintf(q, "&gt;"); q+=4; ++p; }
			else
			  { *q++ = *p++; }
		  }
		*q = 0;
		write_n(ofd, OBUF, strlen(OBUF));
		tsize -= n;
	  }

	sync();
	close(ofd);
	return(0);
}
