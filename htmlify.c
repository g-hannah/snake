#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <misclib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define IBUF_SIZE	4096
#define OBUF_SIZE	16384

static int		fd, ofd;
static struct stat	statb;
static char		*IBUF = NULL, *OBUF = NULL, *TMP = NULL;
static int		P_PARITY;

static char		PARAGRAPH_TAG[256] = "<p>";
static char		HEADING_TAG[256] = "<h1>";
static char		HEADING_CLOSE[256] = "";

void usage(int) __attribute__ ((__noreturn__));
int get_heading_close(char *, char *) __nonnull ((1,2)) __wur;
int create_page(char *fname, char *out) __nonnull ((1,2)) __wur;

int
main(int argc, char *argv[])
{
	static char		c;

	while ((c = getopt(argc, argv, "p:H:h")) != -1)
	  {
		switch(c)
		  {
			case(0x70):
			assert(strlen(optarg) < 256);
			strncpy(PARAGRAPH_TAG, optarg, 256);
			break;
			case(0x48):
			assert(strlen(optarg) < 256);
			strncpy(HEADING_TAG, optarg, 256);
			break;
			case(0x68):
			usage(EXIT_SUCCESS);
			break;
			default:
			usage(EXIT_FAILURE);
		  }
	  }

	if (get_heading_close(HEADING_TAG, HEADING_CLOSE) == -1)
	  { fprintf(stderr, "main() > get_heading_close()\n"); exit(0xff); }

	fprintf(stdout,
		"Paragraph tag: %s\n"
		"Heading tag: %s\n"
		"Heading close: %s\n",
		PARAGRAPH_TAG,
		HEADING_TAG,
		HEADING_CLOSE);

	if (!argv[optind] || !argv[optind+1])
		usage(EXIT_FAILURE);

	if (create_page(argv[optind], argv[optind+1]) < 0)
	  { fprintf(stderr, "main() > create_page()"); }

	exit(EXIT_SUCCESS);
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
	sprintf(OBUF, "%s", PARAGRAPH_TAG);
	P_PARITY = 1;

	n = read_n(fd, IBUF, statb.st_size);
	q = (OBUF + strlen(PARAGRAPH_TAG));
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
							if (strncasecmp(PARAGRAPH_TAG, a, strlen(PARAGRAPH_TAG)) != 0)
							  { --a; continue; }
							else
							  { break; }
						  }

						a += strlen(PARAGRAPH_TAG);
						if (!(TMP = malloc((q - a)+1)))
							return(-1);

						strncpy(TMP, a, (q - a));
						TMP[(q - a)] = 0;
						a -= strlen(PARAGRAPH_TAG);

						strcpy(a, HEADING_TAG);
						P_PARITY = 0;
						a += strlen(HEADING_TAG);

						strncpy(a, TMP, strlen(TMP));
						a += strlen(TMP);
						free(TMP);
						q = a;

						sprintf(q, "%s\n%s", HEADING_CLOSE, PARAGRAPH_TAG);
						P_PARITY = 1;

						q += (strlen(HEADING_CLOSE) + strlen(PARAGRAPH_TAG) + 1);

						while ((*p == 0x0a || *p == 0x0d) && p < (IBUF + statb.st_size))
							++p;
						nlcnt &= ~nlcnt;
						if (p >= (IBUF + statb.st_size)) // not IBUF_SIZE
							break;
					  }
					else // nlcnt != 0
					  {
						sprintf(q, "</p>\n%s", PARAGRAPH_TAG);
						q += (5 + strlen(PARAGRAPH_TAG));
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

		if (P_PARITY == 1)
		  { strncpy(q, "</p>", 4); q += 4; }
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

int
get_heading_close(char *TAG, char *CTAG)
{
	static char		*p = NULL, *q = NULL;

	p = TAG;
	q = CTAG;

	strncpy(q, "</h", 3);
	q += 3;

	while (! isdigit(*p) && p < (TAG + strlen(TAG)))
		++p;

	if (! isdigit(*p))
		return(-1);

	*q++ = *p++;
	strncpy(q, ">", 1);
	++q;
	*q = 0;

	return(0);
}

void
usage(int exit_type)
{
	fprintf(stderr,
		"htmlify <options> <in file> <out file>\n"
		"\n"
		" -p	specify paragraph tag, e.g., \"<p id=\"id_of_p\">\"\n"
		" -H	specify heading tag (default is <h1>)\n"
		" -h	display this information menu\n");

	exit(exit_type);
}
