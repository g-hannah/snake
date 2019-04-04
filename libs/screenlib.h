#ifndef _SCREENLIB_H
#define _SCREENLIB_H	1

# include <errno.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <sys/ioctl.h>
# include <unistd.h>

/* background colours */
# define WHITE		"\e[48;5;15m"
# define RED		"\e[48;5;9m"
# define BLUE		"\e[48;5;12m"
# define DARK_BLUE	"\e[48;5;20m"
# define LIGHT_BLUE	"\e[48;5;33m"
# define SKY_BLUE	"\e[48;5;153m"
# define GREEN		"\e[48;5;10m"
# define DARK_GREEN	"\e[48;5;22m"
# define PINK		"\e[48;5;13m"
# define CYAN		"\e[48;5;14m"
# define YELLOW		"\e[48;5;11m"
# define PURPLE		"\e[48;5;5m"
# define GREY		"\e[48;5;8m"
# define BLACK		"\e[48;5;0m"
# define LEMON		"\e[48;5;229m"
# define AQUA		"\e[48;5;45m"
# define DARK_RED	"\e[48;5;88m"
# define CREAM		"\e[48;5;230m"
# define VIOLET		"\e[48;5;183m"
# define TEAL		"\e[48;5;30m"
# define ORANGE		"\e[48;5;208m"
# define PEACH		"\e[48;5;215m"
# define DARK_GREY	"\e[48;5;240m"
# define LIGHT_GREY	"\e[48;5;252m"
# define SALMON		"\e[48;5;217m"
# define OLIVE		"\e[48;5;100m"

/* text colours */
# define TWHITE		"\e[38;5;15m"
# define TRED		"\e[38;5;9m"
# define TBLUE		"\e[38;5;12m"
# define TDARK_BLUE	"\e[38;5;20m"
# define TLIGHT_BLUE	"\e[38;5;33m"
# define TSKY_BLUE	"\e[38;5;153m"
# define TGREEN		"\e[38;5;10m"
# define TDARK_GREEN	"\e[38;5;22m"
# define TPINK		"\e[38;5;13m"
# define TCYAN		"\e[38;5;14m"
# define TYELLOW		"\e[38;5;11m"
# define TPURPLE		"\e[38;5;5m"
# define TGREY		"\e[38;5;8m"
# define TBLACK		"\e[38;5;0m"
# define TLEMON		"\e[38;5;229m"
# define TAQUA		"\e[38;5;45m"
# define TDARK_RED	"\e[38;5;88m"
# define TCREAM		"\e[38;5;230m"
# define TVIOLET		"\e[38;5;183m"
# define TTEAL		"\e[38;5;30m"
# define TORANGE		"\e[38;5;208m"
# define TPEACH		"\e[38;5;215m"
# define TDARK_GREY	"\e[38;5;240m"
# define TLIGHT_GREY	"\e[38;5;252m"
# define TSALMON		"\e[38;5;217m"
# define TOLIVE		"\e[38;5;100m"

extern struct __winsize;

extern void clear_screen(char *); __nonnull ((1));

// clear a line of length LINE of background colour BGCOL;
// for x-axis, nonzero DIR means clear from right to left;
// for y-axis, nonzero DIR means clear form down to up;
// the original position of the print cursor is restored after the line is cleared.
extern void clear_line_x(char *bgcol, int len, int dir) __nonnull ((1));
extern void clear_line_y(char *bgcol, int len, int dir) __nonnull ((1));

// clear entire horizontal line
extern void clear_line(char *) __nonnull ((1));
extern void up(int);
extern void down(int);
extern void left(int);
extern void right(int);

/* the position of the print cursor is tracked when using up(), down(), left(), right() in relation
 * to the bottom left corner (our determined origin); reset_cursor will put it back to the
 * origin
 */
extern void reset_cursor(void);

/* draw a horizontal line of length LEN and background colour BGCOL
 * if DIR is nonzero, draw the line from right to left,
 * otherwise from left to right
 */
extern void draw_line_x(char *bgcol, int len, int dir) __nonnull ((1));
extern void draw_line_y(char *bgcol, int len, int dir) __nonnull ((1));

#endif // _SCREENLIB_H
