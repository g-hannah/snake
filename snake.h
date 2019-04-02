#include <screenlib.h>
#include <assert.h>
#include <fcntl.h>
#ifndef _ERRNO_H
# include <errno.h>
#endif
#include <fcntl.h>
#include <pthread.h>
#include <termios.h>
#include <stdarg.h>
#include <signal.h>
#include <setjmp.h>

#ifndef _SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#include <sys/stat.h>

#ifndef _TIME_H
# include <time.h>
#endif

#define MAXLINE		1024

struct Snake_Piece
{
	int	l;
	char	d;
	int	apex_r;
	int	apex_u;
	struct Snake_Piece	*next;
	struct Snake_Piece	*prev;
};

struct Snake_Head
{
	int	r;
	int	u;
	int	np;
	int	sl; // overall length of snake
	struct Snake_Piece *h;
};

struct Snake_Tail
{
	int	r;
	int	u;
	struct Snake_Piece *t;
};

struct Food
{
	int	r;
	int	u;
	int	maxu;
	int	minu;
	int	maxr;
	int	minr;
};

struct Player
{
	char		name[32];
	int		score;
	int		num_eaten;
	int		highest_level;
	int		position;
	time_t		when;
	struct Player	*next;
	struct Player	*prev;
};

struct Player_List
{
	struct Player	*first;
	struct Player	*last;
};

typedef struct Snake_Piece Snake_Piece;
typedef struct Snake_Head Snake_Head;
typedef struct Snake_Tail Snake_Tail;
typedef struct Food Food;
typedef struct Player Player;
typedef struct Player_List Player_List;

/* threads */
void *snake_thread(void *);
void *update_sleep(void *);
void *put_some_food(void *);
void *get_direction(void *);
void *track_score(void *);
