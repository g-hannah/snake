#include <screenlib.h>

struct winsize		__winsize;

static int		__r, __u;

static void
__attribute__ ((constructor)) __screenlib_init(void)
{
	__r &= ~__r;
	__u &= ~__u;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &__winsize) < 0)
	  {
		fprintf(stderr, "[libscreenlib.so] __screenlib_init > ioctl (%s)\n",
			strerror(errno));
		exit(EXIT_FAILURE);
	  }

	setvbuf(stdout, NULL, _IONBF, 0);
}

void
clear_line_x(char *bg_colour, int len, int dir)
{
	int		i;
	int		rsave;

	if (len <= 0) return;

	rsave = __r;

	for (i = 0; i < len; ++i)
	  {
		if (dir)
		  { printf("%s%c%c%c\e[m", bg_colour, 0x20, 0x08, 0x08); --__r; }
		else
		  { printf("%s%c\e[m", bg_colour, 0x20); ++__r; }
	  }

	if (rsave < __r) left(__r - rsave);
	else if (rsave > __r) right(rsave - __r);
	else;

	__r = rsave;

	return;
}

void
clear_line_y(char *bg_colour, int len, int dir)
{
	int		i;
	int		usave;

	if (len <= 0) return;

	usave = __u;

	for (i = 0; i < len; ++i)
	  {
		if (dir)
		  { printf("%s%c%c\e[A\e[m", bg_colour, 0x20, 0x08); ++__u; }
		else
		  { printf("%s%c%c\e[B\e[m", bg_colour, 0x20, 0x08); --__u; }
	  }

	if (usave > __u) up(usave - __u);
	else if (usave < __u) down(__u - usave);
	else;

	__u = usave;

	return;
}

void
clear_line(char *bg_colour)
{
	int		i;

	putchar(0x0d);

	for (i = 0; i < __winsize.ws_col; ++i)
		printf("%s%c\e[m", bg_colour, 0x20);

	putchar(0x0d);

	return;
}

void
clear_screen(char *bg_colour)
{
	int		i, j;

	reset_cursor();
	for (i = 0; i < __winsize.ws_row-1; ++i)
	  { printf("\e[A"); ++__u; }

	for (i = 0; i < __winsize.ws_row; ++i)
	  {
		for (j = 0; j < __winsize.ws_col; ++j)
		  {
			printf("%s%c\e[m", bg_colour, 0x20);
			++__r;
			if (j == (__winsize.ws_col-1))
			  { putchar(0x08); --__r; }
		  }
		if (i < (__winsize.ws_row-1))
		  { printf("\r\n"); --__u; __r &= ~__r; }
	  }
	reset_cursor();

	return;
}

void
draw_line_x(char *colour, int len, int dir)
{
	int		i;

	if (len <= 0) return;

	if (dir)
	  {
		for (i = 0; i < (len-1); ++i)
		  { printf("\e[D"); --__r; }
		for (i = 0; i < len; ++i)
		  { printf("%s%c\e[m", colour, 0x20); ++__r; }
		for (i = 0; i < (len+1); ++i)
		  { printf("\e[D"); --__r; }
	  }
	else
	  {
		for (i = 0; i < len; ++i)
		  { printf("%s%c\e[m", colour, 0x20); ++__r; }
	  }

	return;
}

void
draw_line_y(char *colour, int len, int dir)
{
	int		i;

	if (len <= 0) return;

	for (i = 0; i < len; ++i)
	  {
		if (dir)
		  { printf("%s%c%c\e[A\e[m", colour, 0x20, 0x08); ++__u; }
		else
		  { printf("%s%c%c\e[B\e[m", colour, 0x20, 0x08); --__u; }
	  }

	return;
}

void
center_x(int _left, int _right)
{
	int		i;

	for (i = 0; i < ((__winsize.ws_col/2)-_left+_right); ++i)
	  { printf("\e[C"); ++__r; }

	return;
}

void
center_y(int _up, int _down)
{
	int		i;

	for (i = 0; i < ((__winsize.ws_row/2)-_down+_up); ++i)
	  { printf("\e[A"); ++__u; }

	return;
}

void
right(int _x)
{
	int		i;

	if (_x <= 0) return;

	for (i = 0; i < _x; ++i)
	  { printf("\e[C"); ++__r; }

	return;
}

void
left(int _x)
{
	int		i;

	if (_x <= 0) return;

	for (i = 0; i < _x; ++i)
	  { printf("\e[D"); --__r; }

	return;
}

void
up(int _x)
{
	int		i;

	if (_x <= 0) return;

	for (i = 0; i < _x; ++i)
	  { printf("\e[A"); ++__u; }

	return;
}

void
down(int _x)
{
	int		i;

	if (_x <= 0) return;

	for (i = 0; i < _x; ++i)
	  { printf("\e[B"); --__u; }

	return;
}

void
reset_cursor(void)
{
	if (__u > 0) { down(__u); __u &= ~__u; }
	if (__r > 0) { left(__r); __r &= ~__r; }

	return;
}

void
reset_right(void)
{
	putchar(0x0d);
	__r &= ~__r;

	return;
}

void
reset_up(void)
{
	if (__u > 0) { down(__u); __u &= ~__u; }

	return;
}
