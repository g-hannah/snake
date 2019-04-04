#include "snake.h"

#define log_mutex(str, m) \
{\
	;	\
}

int			DEBUG;
FILE			*debug_fp = NULL;

FILE			*hs_fp = NULL;
int			hs_fd = -1;
char			*user_home = NULL;
char			*high_score_file = NULL;
int			path_max;
char			*tmp = NULL;

Player_List		*player_list;
Player			*player;

char			save_name[32];

struct termios		nterm, oterm;
struct winsize		ws;
int			row_2, row_3, row_23, row_4, row_8, row_16, row_max;
int			col_2, col_3, col_23, col_4, col_8, col_16, col_max;
struct Snake_Head	shead;
struct Snake_Tail	stail;
Food			f;

int			default_r;
int			default_u;

/* thread-related variables */
pthread_t		tid_main;
pthread_t		tid_snake;
pthread_t		tid_food;
pthread_t		tid_dir;
pthread_t		tid_score;

volatile sig_atomic_t	tid_food_end;
volatile sig_atomic_t	tid_dir_end;
volatile sig_atomic_t	tid_score_end;
volatile sig_atomic_t	put_some_food_end;

pthread_attr_t		attr;
pthread_mutex_t		mutex;
pthread_mutex_t		smutex;
pthread_mutex_t		sleep_mutex;
pthread_mutex_t		dir_mutex;
pthread_mutex_t		in_mutex;
pthread_mutex_t		list_mutex;

sigjmp_buf		main_thread_env;

char			*BG_COL = NULL;
char			*SN_COL = NULL;
char			*BR_COL = NULL;
char			*HD_COL = NULL;
char			*FD_COL = NULL;
char			*MENU_BG_COL = NULL;
char			*TEXT_SHADOW_COL = NULL;
int			DEFAULT_USLEEP_TIME;
int			USLEEP_TIME;
int			FOOD_REFRESH_TIME;
int			maxr, minr, maxu, minu;
char			DIRECTION;
volatile int		EATEN;
volatile int		level;
volatile int		featen;
volatile int		score;
volatile int		ate_food;
volatile int		gameover;
volatile int		thread_failed;
int			LEVEL_THRESHOLD;

int			**matrix;

/* player stats-related variables */
int BEAT_OWN_SCORE;
int NEW_BEST_PLAYER;

void log_err(char *, ...) __nonnull ((1));
void debug(char *, ...) __nonnull ((1));

static int get_default_sleep_time(void) __wur;
static inline void write_stats(void);
static int write_high_scores(Player *, Player *) __nonnull ((1,2)) __wur;
static int get_high_scores(Player **, Player **) __nonnull ((1,2)) __wur;
static void free_high_scores(Player **, Player **) __nonnull ((1,2));
static Player *new_player_node(void) __wur;
static int show_hall_of_fame(Player *, Player *) __nonnull ((1,2)) __wur;
static int check_current_player_score(Player *, Player *, Player *) __nonnull ((1,2,3)) __wur;

static void save_screen_format(Snake_Head *, Snake_Tail *) __nonnull ((1,2));
static void restore_screen_format(void);

static int title_screen(void);

static void grow_snake(Snake_Head *, Snake_Tail *) __nonnull ((1,2));
static void grow_head(Snake_Head *) __nonnull ((1));
static void grow_tail(Snake_Tail *) __nonnull ((1));
static void snip_tail(Snake_Tail *) __nonnull ((1));
static void adjust_tail(Snake_Tail *, Snake_Head *) __nonnull ((1,2));
static void go_to_head(Snake_Head *, Snake_Tail *) __nonnull ((1,2));
static void go_to_tail(Snake_Head *, Snake_Tail *) __nonnull ((1,2));
static void calibrate_snake_position(Snake_Head *, Snake_Tail *) __nonnull ((1,2));

static int hit_own_body(Snake_Head *, Snake_Tail *) __nonnull ((1,2)) __wur;
static int within_snake(Food *) __nonnull ((1)) __wur;

static int setup_game(void);
static int game_over(void);
static void change_level(int);
static void reset_snake(Snake_Head *, Snake_Tail *) __nonnull ((1,2));
//static void redraw_snake(Snake_Head *, Snake_Tail *) __nonnull ((1,2));
static void _pause(void);
static void unpause(void);

static void level_one(void);
static void level_two(void);
static void level_three(void);
static void level_four(void);
static void level_five(void);

void strip_crnl(char *) __nonnull ((1));
//static char *get_colour(char *) __nonnull ((1)) __wur;

static void
__attribute__ ((constructor)) snake_init(void)
{
	int		ret;
	int		tfd;

	DEBUG &= ~DEBUG;

	/* seed random number generator */
	srand(time(NULL));

	if ((tfd = open("/dev/tty", O_RDWR)) < 0)
	  { log_err("snake_init: failed to open /dev/tty"); goto fail; }

	memset(&oterm, 0, sizeof(oterm));
	memset(&nterm, 0, sizeof(nterm));

	/* change terminal driver attributes */
	tcgetattr(tfd, &oterm);
	memcpy(&nterm, &oterm, sizeof(oterm));
	nterm.c_lflag &= ~(ECHO|ECHOCTL|ICANON);
	tcsetattr(tfd, TCSANOW, &nterm);
	close(tfd);

	/* zero out data structures */
	memset(&f, 0, sizeof(f));
	memset(&ws, 0, sizeof(ws));

	/* initialise thread-related variables */
	debug("initialising thread mutexes");
	if ((ret = pthread_mutex_init(&mutex, NULL)) != 0)
	  { fprintf(stderr, "snake_init: pthread_mutex_init error (%s)\n", strerror(ret)); goto fail; }
	if ((ret = pthread_mutex_init(&smutex, NULL)) != 0)
	  { fprintf(stderr, "snake_init: pthread_mutex_init error (%s)\n", strerror(ret)); goto fail; }
	if ((ret = pthread_mutex_init(&sleep_mutex, NULL)) != 0)
	  { fprintf(stderr, "snake_init: pthread_mutex_init error (%s)\n", strerror(ret)); goto fail; }
	if ((ret = pthread_mutex_init(&dir_mutex, NULL)) != 0)
	  { fprintf(stderr, "snake_init: pthread_mutex_init error (%s)\n", strerror(ret)); goto fail; }
	if ((ret = pthread_mutex_init(&in_mutex, NULL)) != 0)
	  { fprintf(stderr, "snake_init: pthread_mutex_init error (%s)\n", strerror(ret)); goto fail; }
	if ((ret = pthread_mutex_init(&list_mutex, NULL)) != 0)
	  { fprintf(stderr, "snake_init: pthread_mutex_init error (%s)\n", strerror(ret)); goto fail; }

	debug("initialising thread attributes");
	if ((ret = pthread_attr_init(&attr)) != 0)
	  { fprintf(stderr, "snake_init: pthread_attr_init  error (%s)\n", strerror(ret)); goto fail; }
	if ((ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) != 0)
	  { fprintf(stderr, "snake_init: pthread_attr_setdetachstate error (%s)\n", strerror(ret)); goto fail; }

	level = 1;
	// _thresh
	LEVEL_THRESHOLD = 30;
	gameover &= ~gameover;

	tid_food_end = 0;
	tid_dir_end = 0;
	tid_score_end = 0;
	put_some_food_end = 0;

	memset(&shead, 0, sizeof(shead));
	memset(&stail, 0, sizeof(stail));

	path_max &= ~path_max;
	if ((path_max = pathconf("/", _PC_PATH_MAX)) == 0)
		path_max = 1024;

	debug("allocating memory for high_score_file");
#ifndef WIN32
	if (posix_memalign((void **)&high_score_file, 16, path_max) < 0)
	  { log_err("snake_init: posix_memalign error for high_score_file"); goto fail; }
#else
	if (!(high_score_file = calloc(path_max, 1)))
	  { log_err("snake_init: calloc error for high_score_file"); goto fail; }
#endif
	debug("high_score_file = %p", high_score_file);

	memset(high_score_file, 0, path_max);

	if (!(user_home = getenv("HOME")))
	  { log_err("snake_init: failed to get user's home directory"); goto fail; }

	debug("got user home: \"%s\"", user_home);

	/* first check the existence of the snake directory itself */
	sprintf(high_score_file, "%s/.snake", user_home);

	if (access(high_score_file, F_OK) != 0)
		mkdir(high_score_file, S_IRWXU);

	sprintf(high_score_file, "%s/.snake/high_scores.dat", user_home);
	if (access(high_score_file, F_OK) != 0)
	  {
		if ((hs_fd = open(high_score_file, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU & ~S_IXUSR)) < 0)
		  { log_err("snake_init: failed to create high scores file"); goto fail; }
		debug("created \"%s\"", high_score_file);
	  }

	if (!(hs_fp = fopen(high_score_file, "r+")))
	  { log_err("snake_init: fopen error"); goto fail; }

	debug("opened \"%s\"", high_score_file);

	NEW_BEST_PLAYER &= ~NEW_BEST_PLAYER;
	BEAT_OWN_SCORE &= ~BEAT_OWN_SCORE;

	if (!(player = malloc(sizeof(Player))))
	  { log_err("snake_init: malloc error"); goto fail; }

	if (!(player_list = malloc(sizeof(Player_List))))
	  { log_err("snake_init: malloc error"); goto fail; }

	memset(player, 0, sizeof(*player));
	memset(player_list, 0, sizeof(*player_list));

	player_list->first = NULL;
	player_list->last = NULL;

	player->next = NULL;
	player->prev = NULL;

	if (!(tmp = calloc(MAXLINE, 1)))
	  { log_err("snake_init: calloc error"); goto fail; }

	memset(tmp, 0, MAXLINE);

	FOOD_REFRESH_TIME = 12;

	MENU_BG_COL = LIGHT_GREY;
	TEXT_SHADOW_COL = DARK_RED;

	return;

	fail:
	exit(EXIT_FAILURE);
}

static void
__attribute__ ((destructor)) snake_fini(void)
{
	int		tfd;

	free_high_scores(&player_list->first, &player_list->last);

	tfd = open("/dev/tty", O_RDWR);
	tcgetattr(tfd, &nterm);
	if (!(nterm.c_lflag & ECHO))
		nterm.c_lflag |= ECHO;
	if (!(nterm.c_lflag & ECHOCTL))
		nterm.c_lflag |= ECHOCTL;
	if (!(nterm.c_lflag & ICANON))
		nterm.c_lflag |= ICANON;
	tcsetattr(tfd, TCSANOW, &nterm);
	close(tfd);

	if (high_score_file != NULL) { free(high_score_file); high_score_file = NULL; }
	if (tmp != NULL) { free(tmp); tmp = NULL; }
	if (hs_fp != NULL) { fclose(hs_fp); hs_fp = NULL; }
}

void
main_thread_handle_sig(int signo)
{
	if (signo == SIGINT)
	  {
		if (check_current_player_score(player, player_list->first, player_list->last) < 0);
		if (write_high_scores(player_list->first, player_list->last) < 0);
		thread_failed = 1;
		return;
	  }
	else if (signo == SIGUSR1)
	  {
 		siglongjmp(main_thread_env, 1);
	  }
}

// MAIN
int
main(int argc, char *argv[])
{
	char			c, choice[4];
	struct sigaction	nact, oact;
	int			i, j;
	int			play_again, quit;
	int			tfd;

	/* get terminal window dimensions:
	 *
	 * need to do this here and not in the constructor function
	 * because the terminal dimensions are not known before main
	 * is called (I don't know why, but that is the reason it was
	 * moved back here to main())
	 */

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0)
	  { log_err("main: ioctl TIOCGWINSZ error"); goto fail; }

	maxu = ws.ws_row-2;
	maxr = ws.ws_col-3;
	minu = 1;
	minr = 1;

	row_3 = (ws.ws_row/3);
	row_23 = (row_3 * 2);
	row_2 = (ws.ws_row/2);
	row_4 = (row_2/2);
	row_8 = (row_4/2);
	row_16 = (row_8/2);
	row_max = ws.ws_row;

	col_3 = (ws.ws_col/3);
	col_23 = (col_3 * 2);
	col_2 = (ws.ws_col/2);
	col_4 = (col_2/2);
	col_8 = (col_4/2);
	col_16 = (col_8/2);
	col_max = (ws.ws_col-1);

	USLEEP_TIME = get_default_sleep_time();
	clear_screen(MENU_BG_COL);

	if (get_high_scores(&player_list->first, &player_list->last) == -1)
	  { log_err("main: get_high_scores error"); goto fail; }

	title_screen();
	up(ws.ws_row/2);
	right((ws.ws_col/2)-(strlen("Enter Name")/2));
	printf("%s%sEnter name\e[m", MENU_BG_COL, TRED);
	reset_right();
	reset_up();
	//down(1);
	//right(ws.ws_col/2);
	memset(player->name, 0, 32);

	c = 0;
	i &= ~i;

	while (c != 0x0a)
	  {
		c = fgetc(stdin);

		if (c == 0x0a)
		  {
			if (i == 0)
			  {
				up((ws.ws_row/2)-1);
				clear_line(MENU_BG_COL);
				right((ws.ws_col/2)-(strlen("No username entered!")/2));
				printf("%s%sNo username entered!\e[m", MENU_BG_COL, TBLACK);
				reset_right();
				reset_up();
				c &= ~c;
				continue;
			  }

			break;
		  }
		else if (c == 0x7f)
		  {
			if (i == 0)
				continue;

			--i;
			player->name[i] = 0;
			up((ws.ws_row/2)-1);
			clear_line(MENU_BG_COL);
			right((ws.ws_col/2)-(strlen(player->name)/2));
			printf("%s%s%s\e[m", MENU_BG_COL, TPINK, player->name);
			reset_right();
			reset_up();
		  }
		else
		  {
			if (i == 18)
				continue;
			player->name[i++] = c;
			up((ws.ws_row/2)-1);
			clear_line(MENU_BG_COL);
			right((ws.ws_col/2)-(strlen(player->name)/2));
			printf("%s%s%s\e[m", MENU_BG_COL, TPINK, player->name);
			reset_right();
			reset_up();
		  }
	  }

	player->name[i] = 0;

	player->score = 0;
	player->num_eaten = 0;
	player->highest_level = 1;

	reset_right();
	reset_up();

	/* set up signal handler */
	memset(&nact, 0, sizeof(nact));
	memset(&oact, 0, sizeof(oact));

	nact.sa_handler = main_thread_handle_sig;
	nact.sa_flags = 0;
	sigemptyset(&nact.sa_mask);
	if (sigaction(SIGUSR1, &nact, &oact) < 0)
	  { fprintf(stderr, "main: failed to establish signal handler for SIGUSR1\n"); goto fail; }

	while ((c = getopt(argc, argv, "D")) != -1)
	  {
		switch(c)
		  {
			case(0x44):
			DEBUG = 1;
			break;
			default:
			errno = EINVAL;
			log_err("unknown option...");
			exit(EXIT_FAILURE);
		  }
	  }

	/* start the snake in the upper-right quadrant */
	default_r = (((rand()%(ws.ws_col/2))+(ws.ws_col/2))-1);
	default_u = (((rand()%(ws.ws_row/2))+(ws.ws_row/2))-1);

	/* allocate memory for screen matrix */
	if (!(matrix = calloc(ws.ws_row, sizeof(int *))))
	  { log_err("main: calloc error (matrix)"); goto fail; }
	for (i = 0; i < ws.ws_row; ++i)
	  {
		matrix[i] = NULL;
		if (!(matrix[i] = calloc(ws.ws_col, sizeof(int))))
		  { log_err("main: matrix creation failed (calloc)"); goto fail; }

		for (j = 0; j < ws.ws_col; ++j)
			matrix[i][j] = 0;
	  }

	game_loop:
	tid_dir_end = 0;
	tid_food_end = 0;
	tid_score_end = 0;

	if (setup_game() == -1)
		goto fail;

	for(;;)
	  {
		if (gameover)
		  {
			int		l1, l2;
			struct termios	term;

			tid_dir_end = 1;
			tid_score_end = 1;
			tid_food_end = 1;
			EATEN = 1; // ensures first thing done is check value of tid_food_end

			pthread_kill(tid_food, SIGUSR2);
			pthread_kill(tid_dir, SIGQUIT);
			usleep(5000);

			gameover &= ~gameover;

			player->score &= ~(player->score);
			player->num_eaten &= ~(player->num_eaten);

			log_mutex("lock", "mutex");
			pthread_mutex_lock(&mutex);

			memset(&term, 0, sizeof(term));
			tfd = open("/dev/tty", O_RDWR);
			tcgetattr(tfd, &term);
			if (term.c_lflag & ICANON)
				term.c_lflag &= ~(ICANON);

			tcsetattr(tfd, TCSANOW, &term);

			reset_right();
			reset_up();

			l1 = strlen("Play");
			l2 = strlen("Quit");

			play_again = 1;
			quit &= ~quit;

			up((ws.ws_row/2)-(ws.ws_row/16));
			right((ws.ws_col/3) - l1/2);
			printf("%s%sPlay\e[m", RED, TWHITE);
			reset_right();
			right(((ws.ws_col/3)*2) - l2/2);
			printf("%s%sQuit\e[m", MENU_BG_COL, TWHITE);
			reset_right();
			reset_up();

			for (i = 0; i < 4; ++i)
				choice[i] &= ~(choice[i]);

			int	ret;

			for (;;)
			  {
				if ((ret = read(STDIN_FILENO, choice, 3)) <= 0)
				  {
					if (errno == EINTR) continue;
					else { log_err("main: read error"); goto fail; }
				  }
				if (choice[0] == 0x0a)
				  {
					log_mutex("unlock", "mutex");
					pthread_mutex_unlock(&mutex);
					if (play_again)
					  {
						memset(save_name, 0, 32);
						strncpy(save_name, player->name, strlen(player->name));
						save_name[strlen(player->name)] = 0;

						free_high_scores(&player_list->first, &player_list->last);

						if (!(player = malloc(sizeof(Player))))
						  { log_err("main: malloc error"); goto fail; }

						memset(player, 0, sizeof(*player));
						strncpy(player->name, save_name, strlen(save_name));

						player->next = NULL;
						player->prev = NULL;

						player->highest_level = 1;

						if (get_high_scores(&player_list->first, &player_list->last) == -1)
						  { log_err("main: get_high_scores error"); goto fail; }

						EATEN &= ~EATEN;
						level = 1;
						goto game_loop;
					  }
					else
						goto success;
		  	  	  }

				if (strncmp("\e[D", choice, 3) == 0)
				  {
					up((ws.ws_row/2)-(ws.ws_row/16));
					right((ws.ws_col/3)-l1/2);
					printf("%s%sPlay\e[m", RED, TWHITE);
					reset_right();
					right(((ws.ws_col/3)*2)-l2/2);
					printf("%s%sQuit\e[m", MENU_BG_COL, TWHITE);
					play_again = 1;
					quit &= ~quit;
					for (i = 0; i < 4; ++i) choice[i] = 0;
					reset_right();
					reset_up();
				  }
				else if (strncmp("\e[C", choice, 3) == 0)
				  {
					up((ws.ws_row/2)-(ws.ws_row/16));
					right((ws.ws_col/3)-l1/2);
					printf("%s%sPlay\e[m", MENU_BG_COL, TWHITE);
					reset_right();
					right(((ws.ws_col/3)*2)-l2/2);
					printf("%s%sQuit\e[m", RED, TWHITE);
					quit = 1;
					play_again &= ~play_again;
					for (i = 0; i < 4; ++i) choice[i] = 0;
					reset_right();
					reset_up();
				  }
				else
				  {
					for (i = 0; i < 4; ++i) choice[i] = 0;
					continue;
				  }
			  }
		  }
		if (thread_failed)
		  {
			pthread_kill(tid_snake, SIGINT);
			pthread_kill(tid_food, SIGINT);
			sigaction(SIGUSR1, &oact, NULL);
			goto fail;
		  }
	  }

	success:
	exit(EXIT_SUCCESS);

	fail:
	exit(EXIT_FAILURE);
}

// SETUP_GAME
int
setup_game(void)
{
	pthread_mutex_lock(&smutex);

	/* start with a single piece; head and tail both point to same piece */

	EATEN &= ~EATEN;

	if (!shead.h)
	  {
		shead.h = malloc(sizeof(Snake_Piece));
		memset(shead.h, 0, sizeof(*(shead.h)));
		shead.h->d = 0x6c;
		shead.h->prev = NULL;
		shead.h->next = NULL;
		stail.t = shead.h;
	  }

	log_mutex("unlock", "smutex");
	pthread_mutex_unlock(&smutex);

	if (pthread_attr_init(&attr) != 0)
		goto fail;
	if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
		goto fail;

	shead.sl = 2;
	change_level(1);
	//level_one();

	thread_failed &= ~thread_failed;
	/* create threads */
	if (pthread_create(&tid_snake, &attr, snake_thread, NULL) != 0)
		goto fail;

	tid_main = pthread_self();

	if (pthread_create(&tid_food, &attr, put_some_food, NULL) != 0)
		goto fail;
	if (pthread_create(&tid_dir, &attr, get_direction, NULL) != 0)
		goto fail;
	if (pthread_create(&tid_score, &attr, track_score, NULL) != 0)
		goto fail;

	return(0);

	fail:
	fprintf(stderr, "setup_game: failed to setup game\n");
	return(-1);
}

// SNAKE_THREAD
void *
snake_thread(void *arg)
{
	int sleeptime;

	restart:
	sleep(1);

	switch(shead.h->d)
	  {
		case(0x6c):
		break;
		case(0x72):
		goto go_right;
		break;
		case(0x75):
		goto go_up;
		break;
		case(0x64):
		goto go_down;
		break;
	  }

	go_left:
	log_mutex("lock", "smutex");
	pthread_mutex_lock(&smutex);
	if (shead.h->d != 0x6c)
		shead.h->d = 0x6c;
	log_mutex("unlock", "smutex");
	pthread_mutex_unlock(&smutex);

	log_mutex("lock", "dir_mutex");
	pthread_mutex_lock(&dir_mutex);
	DIRECTION = 0x6c;
	log_mutex("unlock", "dir_mutex");
	pthread_mutex_unlock(&dir_mutex);

	for (;;)
	  {
		log_mutex("lock", "smutex");
		pthread_mutex_lock(&mutex);
		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);

		left(1);
		draw_line_x(HD_COL, 1, 0);
		--(shead.r);
		draw_line_x(SN_COL, 1, 0);
		left(2);
		if (shead.np > 1)
			++(shead.h->l);

		log_mutex("unlock", "smutex");
		pthread_mutex_unlock(&smutex);
		log_mutex("unlock", "mutex");
		pthread_mutex_unlock(&mutex);

		if (matrix[shead.u][shead.r] == -1)
		//if (shead.r < minr)
			goto lose;

		adjust_tail(&stail, &shead);

		log_mutex("lock", "sleep_mutex");
		pthread_mutex_lock(&sleep_mutex);
		sleeptime = USLEEP_TIME;
		log_mutex("unlock", "sleep_mutex");
		pthread_mutex_unlock(&sleep_mutex);

		usleep(sleeptime);

		if (hit_own_body(&shead, &stail))
			goto lose;

		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);
		if (shead.r == f.r && shead.u == f.u) // we ate food
		  {
			log_mutex("unlock", "smutex");
			pthread_mutex_unlock(&smutex);
			EATEN = 1;
			ate_food = 1;
			pthread_kill(tid_food, SIGUSR2);
			grow_snake(&shead, &stail);

			++(player->num_eaten);
			if (player->num_eaten != 0 && (player->num_eaten % LEVEL_THRESHOLD) == 0)
			  {
				++level;
				change_level(level);
				goto restart;
			  }

		  }
		else
		  { log_mutex("unlock", "smutex"); pthread_mutex_unlock(&smutex); }

		log_mutex("lock", "dir_mutex");
		pthread_mutex_lock(&dir_mutex);
		if (DIRECTION != 0x6c)
		  {
			switch(DIRECTION)
			  {
				case(0x75):
				grow_head(&shead);
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				goto go_up;
				break;
				case(0x64):
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				grow_head(&shead);
				goto go_down;
				break;
				default:
				DIRECTION = 0x6c;
			  }
		  }
		log_mutex("unlock", "dir_mutex");
		pthread_mutex_unlock(&dir_mutex);
	  }

	go_up:
	log_mutex("lock", "smutex");
	pthread_mutex_lock(&smutex);
	if (shead.h->d != 0x75)
		shead.h->d = 0x75;
	log_mutex("unlock", "smutex");
	pthread_mutex_unlock(&smutex);

	log_mutex("lock", "dir_mutex");
	pthread_mutex_lock(&dir_mutex);
	DIRECTION = 0x75;
	log_mutex("unlock", "dir_mutex");
	pthread_mutex_unlock(&dir_mutex);

	for (;;)
	  {
		log_mutex("lock", "mutex");
		pthread_mutex_lock(&mutex);
		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);

		up(1);
		draw_line_x(HD_COL, 1, 0);
		++(shead.u);
		left(1);
		down(1);
		draw_line_x(SN_COL, 1, 0);
		left(1);
		up(1);
		if (shead.np > 1)
			++(shead.h->l);

		log_mutex("unlock", "smutex");
		pthread_mutex_unlock(&smutex);
		log_mutex("unlock", "mutex");
		pthread_mutex_unlock(&mutex);

		if (matrix[shead.u][shead.r] == -1)
		//if (shead.u > maxu)
			goto lose;

		adjust_tail(&stail, &shead);

		log_mutex("lock", "sleep_mutex");
		pthread_mutex_lock(&sleep_mutex);
		sleeptime = USLEEP_TIME;
		log_mutex("unlock", "sleep_mutex");
		pthread_mutex_unlock(&sleep_mutex);

		usleep(sleeptime);

		if (hit_own_body(&shead, &stail))
			goto lose;

		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);
		if (shead.r == f.r && shead.u == f.u) // we ate food
		  {
			log_mutex("unlock", "smutex");
			pthread_mutex_unlock(&smutex);
			EATEN = 1;
			ate_food = 1;
			pthread_kill(tid_food, SIGUSR2);
			grow_snake(&shead, &stail);

			++(player->num_eaten);
			if (player->num_eaten != 0 && (player->num_eaten % LEVEL_THRESHOLD) == 0)
			  {
				++level;
				change_level(level);
				goto restart;
			  }

		  }
		else
		  { log_mutex("unlock", "smutex"); pthread_mutex_unlock(&smutex); }

		log_mutex("lock", "dir_mutex");
		pthread_mutex_lock(&dir_mutex);
		if (DIRECTION != 0x75)
		  {
			switch(DIRECTION)
			  {
				case(0x6c):
				grow_head(&shead);
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				goto go_left;
				break;
				case(0x72):
				grow_head(&shead);
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				goto go_right;
				break;
				default:
				DIRECTION = 0x75;
			  }
		  }
		log_mutex("unlock", "dir_mutex");
		pthread_mutex_unlock(&dir_mutex);
	  }

	go_down:
	log_mutex("lock", "smutex");
	pthread_mutex_lock(&smutex);
	if (shead.h->d != 0x64)
		shead.h->d = 0x64;
	log_mutex("unlock", "smutex");
	pthread_mutex_unlock(&smutex);

	log_mutex("lock", "dir_mutex");
	pthread_mutex_lock(&dir_mutex);
	DIRECTION = 0x64;
	log_mutex("unlock", "dir_mutex");
	pthread_mutex_unlock(&dir_mutex);

	for (;;)
	  {
		log_mutex("lock", "mutex");
		pthread_mutex_lock(&mutex);
		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);

		down(1);
		draw_line_x(HD_COL, 1, 0);
		--(shead.u);
		left(1);
		up(1);
		draw_line_x(SN_COL, 1, 0);
		left(1);
		down(1);
		if (shead.np > 1)
			++(shead.h->l);

		log_mutex("unlock", "smutex");
		pthread_mutex_unlock(&smutex);
		log_mutex("unlock", "mutex");
		pthread_mutex_unlock(&mutex);

		if (matrix[shead.u][shead.r] == -1)
		//if (shead.u < minu)
			goto lose;

		adjust_tail(&stail, &shead);

		log_mutex("lock", "sleep_mutex");
		pthread_mutex_lock(&sleep_mutex);
		sleeptime = USLEEP_TIME;
		log_mutex("unlock", "sleep_mutex");
		pthread_mutex_unlock(&sleep_mutex);

		usleep(sleeptime);

		if (hit_own_body(&shead, &stail))
			goto lose;

		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);
		if (shead.r == f.r && shead.u == f.u) // we ate food
		  {
			log_mutex("unlock", "smutex");
			pthread_mutex_unlock(&smutex);
			EATEN = 1;
			ate_food = 1;
			pthread_kill(tid_food, SIGUSR2);
			grow_snake(&shead, &stail);

			++(player->num_eaten);
			if (player->num_eaten != 0 && (player->num_eaten % LEVEL_THRESHOLD) == 0)
			  {
				++level;
				change_level(level);
				goto restart;
			  }
		  }
		else
		  { log_mutex("unlock", "smutex"); pthread_mutex_unlock(&smutex); }

		log_mutex("lock", "dir_mutex");
		pthread_mutex_lock(&dir_mutex);
		if (DIRECTION != 0x64)
		  {
			switch(DIRECTION)
			  {
				case(0x6c):
				grow_head(&shead);
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				goto go_left;
				break;
				case(0x72):
				grow_head(&shead);
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				goto go_right;
				break;
				default:
				DIRECTION = 0x64;
			  }
		  }
		log_mutex("unlock", "dir_mutex");
		pthread_mutex_unlock(&dir_mutex);
	  }

	go_right:
	log_mutex("lock", "smutex");
	pthread_mutex_lock(&smutex);
	if (shead.h->d == 0)
		shead.h->d = 0x72;
	log_mutex("unlock", "smutex");
	pthread_mutex_unlock(&smutex);

	log_mutex("lock", "&dir_mutex");
	pthread_mutex_lock(&dir_mutex);
	DIRECTION = 0x72;
	log_mutex("unlock", "dir_mutex");
	pthread_mutex_unlock(&dir_mutex);

	for (;;)
	  {
		log_mutex("lock", "mutex");
		pthread_mutex_lock(&mutex);
		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);

		right(1);
		draw_line_x(HD_COL, 1, 0);
		left(2);
		++(shead.r);
		draw_line_x(SN_COL, 1, 0);
		if (shead.np > 1)
			++(shead.h->l);

		log_mutex("unlock", "smutex");
		pthread_mutex_unlock(&smutex);
		log_mutex("unlock", "mutex");
		pthread_mutex_unlock(&mutex);

		if (matrix[shead.u][shead.r] == -1)
		//if (shead.r > maxr)
			goto lose;

		adjust_tail(&stail, &shead);

		log_mutex("lock", "sleep_mutex");
		pthread_mutex_lock(&sleep_mutex);
		sleeptime = USLEEP_TIME;
		log_mutex("unlock", "sleep_mutex");
		pthread_mutex_unlock(&sleep_mutex);

		usleep(sleeptime);

		if (hit_own_body(&shead, &stail))
			goto lose;

		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);
		if (shead.r == f.r && shead.u == f.u) // we ate food
		  {
			log_mutex("unlock", "smutex");
			pthread_mutex_unlock(&smutex);
			EATEN = 1;
			ate_food = 1;
			pthread_kill(tid_food, SIGUSR2);
			grow_snake(&shead, &stail);

			++(player->num_eaten);
			if (player->num_eaten != 0 && (player->num_eaten % LEVEL_THRESHOLD) == 0)
			  {
				++level;
				change_level(level);
				goto restart;
			  }

		  }
		else
		  { log_mutex("unlock", "smutex"); pthread_mutex_unlock(&smutex); }

		log_mutex("lock", "dir_mutex");
		pthread_mutex_lock(&dir_mutex);
		if (DIRECTION != 0x72)
		  {
			switch(DIRECTION)
			  {
				case(0x75):
				grow_head(&shead);
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				goto go_up;
				break;
				case(0x64):
				grow_head(&shead);
				log_mutex("unlock", "dir_mutex");
				pthread_mutex_unlock(&dir_mutex);
				goto go_down;
				break;
				default:
				DIRECTION = 0x72;
			  }
		  }
		log_mutex("unlock", "dir_mutex");
		pthread_mutex_unlock(&dir_mutex);
	  }

	lose:
	//pthread_kill(tid_food, SIGINT);
	//pthread_kill(tid_dir, SIGINT);
	if (game_over() == -1)
	  {
		log_err("snake_thread: game_over error");
		pthread_kill(tid_main, SIGINT);
		pthread_exit((void *)-1);
	  }

	gameover = 1;
	pthread_exit((void *)0);
}
// SNAKE_END

// TRACK_SCORE
void *
track_score(void *arg)
{
	while (!tid_score_end)
	  {
		if (ate_food)
		  {
			ate_food &= ~ate_food;
			(player->score += (level * 10));
			pthread_mutex_lock(&mutex);
			write_stats();
			pthread_mutex_unlock(&mutex);
		  }
	  }

	pthread_exit((void *)0);
}

/*void *
update_sleep(void *arg)
{
	for (;;)
	  {
		sleep(25);
		if (USLEEP_TIME > 50000)
		  {
			pthread_mutex_lock(&sleep_mutex);
			USLEEP_TIME -= 10000;
			pthread_mutex_unlock(&sleep_mutex);
		  }
		else
		  {
			sleep(0xdeadbeef);
		  }
	  }
}*/

static void
put_some_food_signal_handler(int signo)
{
	return;
}

// PUT_SOME_FOOD
void *
put_some_food(void *arg)
{
	signal(SIGUSR2, put_some_food_signal_handler);

	memset(&f, 0, sizeof(f));
	f.maxr = ws.ws_col-2;
	f.minr = 1;
	f.maxu = ws.ws_row-1;
	f.minu = 1;
	f.u = f.r = 1;

	while (!tid_food_end)
	  {
		f.r = ((rand()%(f.maxr-1))+1);
		f.u = ((rand()%(f.maxu-1))+1);

		if (within_snake(&f) || (matrix[f.u][f.r] == -1))
		  {
			while (within_snake(&f) || (matrix[f.u][f.r] == -1))
		  	  {
				f.r = ((rand()%(f.maxr-1))+1);
				f.u = ((rand()%(f.maxu-1))+1);
		  	  }
		  }

		EATEN &= ~EATEN;

		log_mutex("lock", "mutex");
		pthread_mutex_lock(&mutex);
		log_mutex("lock", "smutex");
		pthread_mutex_lock(&smutex);

		reset_cursor();
		up(f.u);
		right(f.r);
		draw_line_x(FD_COL, 1, 0);
		reset_cursor();
		up(shead.u);
		right(shead.r);

		log_mutex("unlock", "smutex");
		pthread_mutex_unlock(&smutex);
		log_mutex("unlock", "mutex");
		pthread_mutex_unlock(&mutex);

		sleep(FOOD_REFRESH_TIME);
	
		if (!EATEN)
		  {
			log_mutex("lock", "mutex");
			pthread_mutex_lock(&mutex);
			log_mutex("lock", "smutex");
			pthread_mutex_lock(&smutex);

			reset_cursor();
			up(f.u);
			right(f.r);
			draw_line_x(BG_COL, 1, 0);
			reset_cursor();
			up(shead.u);
			right(shead.r);

			log_mutex("unlock", "smutex");
			pthread_mutex_unlock(&smutex);
			log_mutex("unlock", "mutex");
			pthread_mutex_unlock(&mutex);
		  }
	  }

	pthread_exit((void *)0);
}

/* when this is called, screen mutex is already locked */
// ADJUST_TAIL
void
adjust_tail(Snake_Tail *t, Snake_Head *h)
{
	pthread_mutex_lock(&mutex);
	pthread_mutex_lock(&smutex);

	go_to_tail(h, t);

	draw_line_x(BG_COL, 1, 0);
	left(1);

	if (h->np > 1)
		--(t->t->l);

	switch(t->t->l==0?t->t->next->d:t->t->d)
	  {
		case(0x75):
		up(1);
		++(t->u);
		break;
		case(0x64):
		down(1);
		--(t->u);
		break;
		case(0x6c):
		left(1);
		--(t->r);
		break;
		case(0x72):
		right(1);
		++(t->r);
		break;
	  }

	if (t->t->l == 0)
	  {
		snip_tail(t);
		--(h->np);
	  }

	go_to_head(h, t);

	pthread_mutex_unlock(&smutex);
	pthread_mutex_unlock(&mutex);

	return;
}

// SNIP_TAIL
void
snip_tail(Snake_Tail *t)
{
	t->t = t->t->next;
	free(t->t->prev);
	t->t->prev = NULL;

	return;
}

// GROW_HEAD
void
grow_head(Snake_Head *h)
{
	pthread_mutex_lock(&smutex);
	h->h->next = malloc(sizeof(Snake_Piece));
	memset(h->h->next, 0, sizeof(*(h->h->next)));
	h->h->next->prev = h->h;
	h->h->next->next = NULL;

	h->h->apex_r = h->r;
	h->h->apex_u = h->u;

	h->h = h->h->next;

	h->h->d = 0;
	h->h->l = 0;

	++(h->np);
	pthread_mutex_unlock(&smutex);

	return;
}

// GROW_TAIL
void
grow_tail(Snake_Tail *tail)
{
	tail->t->prev = malloc(sizeof(Snake_Piece));
	memset(tail->t->prev, 0, sizeof(*(tail->t->prev)));

	tail->t->prev->next = tail->t;
	tail->t = tail->t->prev;
	tail->t->prev = NULL;

	tail->t->apex_r = tail->r;
	tail->t->apex_u = tail->u;

	tail->t->l = 0;
	tail->t->d = 0;

	return;
}

// GROW_SNAKE
void
grow_snake(Snake_Head *h, Snake_Tail *t)
{
	int	DONE;

	DONE &= ~DONE;

	pthread_mutex_lock(&mutex);
	pthread_mutex_lock(&smutex);

	go_to_tail(h, t);

	/* we need to grow in opposite direction of t->t->d */
	switch(t->t->d)
	  {
		case(0x72):
		if (t->r > minr)
		  {
			left(1);
			draw_line_x(SN_COL, 1, 0);
			left(1);
			--(t->r);
			++(t->t->l);
			DONE = 1;
		  }
		else
		  {
			grow_tail(t);
			if (t->u < maxu && t->u > minu) // then choose at random
			  {
				if ((rand()%2)&1)
					t->t->d = 0x75;
				else
					t->t->d = 0x64;
			  }
			else
			  {
				if (t->u < maxu)
					t->t->d = 0x75;
				else
					t->t->d = 0x64;
			  }
		  }
		break;
		case(0x6c):
		if (t->r < maxr)
		  {
			right(1);
			draw_line_x(SN_COL, 1, 0);
			left(1);
			++(t->r);
			++(t->t->l);
			DONE = 1;
		  }
		else
		  {
			grow_tail(t);
			if (t->u < maxu && t->u > minu)
			  {
				if ((rand()%2)&1)
					t->t->d = 0x64;
				else
					t->t->d = 0x75;
			  }
			else
			  {
				if (t->u < maxu)
					t->t->d = 0x75;
				else
					t->t->d = 0x64;
			  }
		  }
		break;
		case(0x75):
		if (t->u > minu)
		  {
			down(1);
			draw_line_x(SN_COL, 1, 0);
			left(1);
			--(t->u);
			++(t->t->l);
			DONE = 1;
		  }
		else
		  {
			grow_tail(t);
			if (t->r < maxr && t->r > minr)
			  {
				if ((rand()%2)&1)
					t->t->d = 0x6c;
				else
					t->t->d = 0x72;
			  }
			else
			  {
				if (t->r < maxr)
					t->t->d = 0x72;
				else
					t->t->d = 0x6c;
			  }
		  }
		break;
		case(0x64):
		if (t->u < maxu)
		  {
			up(1);
			draw_line_x(SN_COL, 1, 0);
			left(1);
			++(t->u);
			++(t->t->l);
			DONE = 1;
		  }
		else
		  {
			grow_tail(t);
			if (t->r > minr && t->r < maxr)
			  {
				if ((rand()%2)&1)
					t->t->d = 0x6c;
				else
					t->t->d = 0x72;
			  }
			else
			  {
				if (t->r > minr)
					t->t->d = 0x6c;
				else
					t->t->d = 0x72;
			  }
		  }
		break;
	  }

	++(h->sl);

	pthread_mutex_unlock(&smutex);

	if (DONE)
	  {
		go_to_head(h, t);
		pthread_mutex_unlock(&mutex);
		return;
	  }

	/* due to the way the length of each piece is counted,
	 * we need to consider the final block in the previous
	 * tail piece as belonging to the new tail; so we adjust
	 * the lengths here accordingly
	 */

	pthread_mutex_lock(&smutex);

	--(t->t->next->l);
	++(t->t->l);

	switch(t->t->d)
	  {
		case(0x75):
		down(1);
		--(t->u);
		break;
		case(0x64):
		up(1);
		++(t->u);
		break;
		case(0x6c):
		right(1);
		++(t->r);
		break;
		case(0x72):
		left(1);
		--(t->r);
		break;
	  }

	draw_line_x(SN_COL, 1, 0);
	left(1);
	++(t->t->l);

	go_to_head(h, t);

	pthread_mutex_unlock(&smutex);
	pthread_mutex_unlock(&mutex);
	return;
}

// GO_HEAD
void
go_to_head(Snake_Head *h, Snake_Tail *t)
{
	if (h->u > t->u)
		up(h->u - t->u);
	else if (h->u < t->u)
		down(t->u - h->u);
	else;

	if (h->r > t->r)
		right(h->r - t->r);
	else if (h->r < t->r)
		left(t->r - h->r);
	else;
}

// GO_TAIL
void
go_to_tail(Snake_Head *h, Snake_Tail *t)
{
	if (t->u > h->u)
		up(t->u - h->u);
	else if (t->u < h->u)
		down(h->u - t->u);
	else;

	if (t->r > h->r)
		right(t->r - h->r);
	else if (t->r < h->r)
		left(h->r - t->r);
	else;

	return;
}

static void
get_direction_sig_handler(int signo)
{
	return;
}

// GET_DIRECTION
void *
get_direction(void *arg)
{
	char			c[4];
	int			fd;
	int			i;
	int			ret;
	struct termios		term, old;

	signal(SIGQUIT, get_direction_sig_handler);

	/* switch off terminal driver echo */
	memset(&term, 0, sizeof(term));
	memset(&old, 0, sizeof(old));
	fd = open("/dev/tty", O_RDWR);
	tcgetattr(fd, &old);
	memcpy(&term, &old, sizeof(old));
	term.c_lflag &= ~(ICANON|ECHO|ECHOCTL);
	tcsetattr(fd, TCSANOW, &term);

	for (i = 0; i < 4; ++i)
		c[i] &= ~(c[i]);

	while (!tid_dir_end)
	  {
		while (strncmp("\e[A", c, 3) != 0 &&
		       strncmp("\e[B", c, 3) != 0 &&
		       strncmp("\e[C", c, 3) != 0 &&
		       strncmp("\e[D", c, 3) != 0)
		  {
			if ((ret = read(STDIN_FILENO, &c[0], 1)) != 1)
			  {
				if (errno == EINTR) continue;
				else { log_err("get_direction: read error"); goto fail; }
			  }
			if (c[0] == 0x20)
			  {
				_pause();
				c[0] &= ~(c[0]);
				while (c[0] != 0x20)
				  {
					if ((ret = read(STDIN_FILENO, &c[0], 1)) != 1)
					  {
						if (errno == EINTR) continue;
						else { log_err("get_direction: read error"); goto fail; }
					  }
				  }
				unpause();
				for (i = 0; i < 4; ++i)
					c[i] = 0;
				continue;
			  }
		
			if ((ret = read(STDIN_FILENO, &c[1], 2)) != 2)
			  {
				if (errno == EINTR) continue;
				else { log_err("get_direction: read error"); goto fail; }
			  }
			c[3] = 0;
		  }

		pthread_mutex_lock(&dir_mutex);
		if (strncmp("\e[A", c, 3) == 0)
		  {
			DIRECTION = 0x75;
		  }
		else if (strncmp("\e[B", c, 3) == 0)
		  {
			DIRECTION = 0x64;
		  }
		else if (strncmp("\e[C", c, 3) == 0)
		  {
			DIRECTION = 0x72;
		  }
		else if (strncmp("\e[D", c, 3) == 0)
		  {
			DIRECTION = 0x6c;
		  }

		for (i = 0; i < 3; ++i)
			c[i] &= ~(c[i]);

		pthread_mutex_unlock(&dir_mutex);
		usleep(10000);
	  }

	pthread_exit((void *)0);

	fail:
	pthread_kill(tid_main, SIGINT);
	pthread_exit((void *)-1);
}

int
hit_own_body(Snake_Head *h, Snake_Tail *t)
{
	Snake_Piece	*p = NULL;
	int		HIT;

	HIT &= ~HIT;

	pthread_mutex_lock(&smutex);
	for (p = t->t; p->next != NULL; p = p->next)
	  {
		if (p->d == 0x75)
		  {
			if (h->r == p->apex_r &&
			    h->u <= p->apex_u &&
			    h->u >= (p->prev==NULL?t->u:p->prev->apex_u))
				HIT = 1;
			else
				continue;
		  }
		else if (p->d == 0x64)
		  {
			if (h->r == p->apex_r &&
			    h->u >= p->apex_u &&
			    h->u <= (p->prev==NULL?t->u:p->prev->apex_u))
				HIT = 1;
			else
				continue;
		  }
		else if (p->d == 0x6c)
		  {
			if (h->u == (p->prev==NULL?t->u:p->apex_u) &&
			    h->r >= p->apex_r &&
			    h->r <= (p->prev==NULL?t->r:p->prev->apex_r))
				HIT = 1;
			else
				continue;
		  }
		else
		  {
			if (h->u == p->apex_u &&
			    h->r <= p->apex_r &&
			    h->r >= (p->prev==NULL?t->r:p->prev->apex_r))
				HIT = 1;
			else
				continue;
		  }
	  }

	pthread_mutex_unlock(&smutex);

	return(HIT);
}

int
within_snake(Food *f)
{
	Snake_Piece	*p = NULL;
	int		WITHIN;

	WITHIN &= ~WITHIN;

	pthread_mutex_lock(&smutex);
	for (p = stail.t; p->next != NULL; p = p->next)
	  {
		if (p->d == 0x75)
		  {
			if (f->r == p->apex_r &&
			    f->u <= p->apex_u &&
			    f->u >= (p->prev==NULL?stail.u:p->prev->apex_u))
				WITHIN = 1;
			else
				continue;
		  }
		else if (p->d == 0x64)
		  {
			if (f->r == p->apex_r &&
			    f->u >= p->apex_u &&
			    f->u <= (p->prev==NULL?stail.u:p->prev->apex_u))
				WITHIN = 1;
			else
				continue;
		  }
		else if (p->d == 0x6c)
		  {
			if (f->u == p->apex_u &&
			    f->r >= p->apex_r &&
			    f->r <= (p->prev==NULL?stail.r:p->prev->apex_r))
				WITHIN = 1;
			else
				continue;
		  }
		else
		  {
			if (f->u == p->apex_u &&
			    f->r <= p->apex_r &&
			    f->r >= (p->prev==NULL?stail.r:p->prev->apex_r))
				WITHIN = 1;
			else
				continue;
		  }
	  }

	pthread_mutex_unlock(&smutex);

	return(WITHIN);
}

// GAME_OVER
int
game_over(void)
{
	int		i, j;
	char		*game_over_string = "Game Over";
	char		*your_score_string = "Your Score";
	char		*your_num_eaten_string = "Food Eaten";
	char		*beat_record_string = "You beat the all time record!";
	char		*beat_own_score_string = "You beat your previous score!";
	char		*digits = "0123456789";
	int		char_delay;
	int		game_over_text[8][64] =
	  {
		{ 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0 },
		{ 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0 },
		{ 1, 1, 0, 2, 2, 2, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 1, 0, 1, 1, 0, 2, 1, 1, 0, 2, 2, 2, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 2, 2, 2, 2, 1, 1, 0, 2, 1, 1, 0 },
		{ 1, 1, 0, 2, 2, 2, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 1, 0, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0 },
		{ 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 1, 0, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 0, 2, 2, 2 },
		{ 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 1, 0, 1, 1, 0, 2, 1, 1, 0, 2, 2, 2, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 2, 1, 1, 1, 1, 0, 2, 2, 1, 1, 0, 2, 2, 2, 2, 2, 1, 1, 1, 1, 0, 2, 2 },
		{ 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 1, 0, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 2, 1, 1, 1, 1, 0, 2, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 1, 1, 0, 2 },
		{ 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 1, 0, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 2, 2, 1, 1, 0, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0 }
	  };

	char_delay = 90000;

	clear_screen(MENU_BG_COL);
	if (check_current_player_score(player, player_list->first, player_list->last) < 0)
	  { log_err("game_over: check_current_player error"); goto fail; }

	if (write_high_scores(player_list->first, player_list->last) < 0)
	  { log_err("game_over: write_high_scores error"); goto fail; }

	if (show_hall_of_fame(player_list->first, player_list->last) < 0)
	  { log_err("game_over: show_hall_of_fame error"); goto fail; }

	pthread_mutex_lock(&mutex);
	reset_cursor();

	up((ws.ws_row/2)+(ws.ws_row/4)+(ws.ws_row/8));
	center_x((65/2), 0);

	for (i = 0; i < 8; ++i)
	  {
		for (j = 0; j < 64; ++j)
		  {
			if (game_over_text[i][j] == 1)
				draw_line_x(RED, 1, 0);
			else if (game_over_text[i][j] == 2)
				draw_line_x(MENU_BG_COL, 1, 0);
			else
				draw_line_x(TEXT_SHADOW_COL, 1, 0);
		  }

		reset_right();
		down(1);
		center_x((65/2), 0);
	  }

	reset_right();
	down(1);

	right((ws.ws_col/2) - (strlen(game_over_string)/2));
	printf("%s%s", MENU_BG_COL, TWHITE);
	for (i = 0; i < strlen(game_over_string); ++i)
	  { fputc(game_over_string[i], stdout); usleep(char_delay); }

	printf("\e[m");
	reset_right();
	down(2);

	if (NEW_BEST_PLAYER)
	  {
		right((ws.ws_col/2)-(strlen(beat_record_string)/2) - 4);
		printf("%s%s", MENU_BG_COL, TWHITE);
		for (i = 0; i < strlen(beat_record_string); ++i)
		  { fputc(beat_record_string[i], stdout); usleep(char_delay); }
		printf("\e[m");
		reset_right();
		down(1);
	  }
	else if (BEAT_OWN_SCORE)
	  {
		right((ws.ws_col/2)-(strlen(beat_own_score_string)/2) - 4);
		printf("%s%s", MENU_BG_COL, TWHITE);
		for (i = 0; i < strlen(beat_own_score_string); ++i)
		  { fputc(beat_own_score_string[i], stdout); usleep(char_delay); }
		reset_right();
		down(1);
	  }

	right((ws.ws_col/2)-(strlen(your_score_string)/2)-4);
	printf("%s%s", MENU_BG_COL, TDARK_GREEN);
	for (i = 0; i < strlen(your_score_string); ++i)
	  { fputc(your_score_string[i], stdout); usleep(char_delay); }

	fputc(0x20, stdout);
	usleep(char_delay);

	for (i = 0; i < 8000; ++i)
	  {
		if (player->score == 0)
		  {
			fputc(digits[(rand()%10)], stdout);
			left(1);
		  }
		else if (player->score < 99)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(2);
		  }
		else if (player->score > 99 && player->score < 1000)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(3);
		  }
		else if (player->score > 999 && player->score < 10000)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(4);
		  }
		else if (player->score > 9999 && player->score < 100000)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(5);
		  }
		usleep(50);
	  }

	printf("%d\e[m", player->score);
	reset_right();
	down(1);
	right((ws.ws_col/2)-(strlen(your_num_eaten_string)/2)-4);
	printf("%s%s", MENU_BG_COL, TDARK_GREEN);
	for (i = 0; i < strlen(your_num_eaten_string); ++i)
	  { fputc(your_num_eaten_string[i], stdout); usleep(char_delay); }

	fputc(0x20, stdout);
	usleep(char_delay);

	for (i = 0; i < 8000; ++i)
	  {
		if (player->num_eaten < 10)
		  {
			fputc(digits[(rand()%10)], stdout);
			left(1);
		  }
		else if (player->num_eaten > 9 && player->num_eaten < 99)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(2);
		  }
		else if (player->num_eaten > 99 && player->num_eaten < 1000)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(3);
		  }
		else if (player->num_eaten > 999 && player->num_eaten < 10000)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(4);
		  }
		else if (player->num_eaten > 9999 && player->num_eaten < 100000)
		  {
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			fputc(digits[(rand()%10)], stdout);
			left(5);
		  }
		usleep(50);
	  }

	printf("%d\e[m", player->num_eaten);

	reset_right();
	reset_up();
	pthread_mutex_unlock(&mutex);

	NEW_BEST_PLAYER &= ~NEW_BEST_PLAYER;
	BEAT_OWN_SCORE &= ~BEAT_OWN_SCORE;

	return(0);

	fail:
	return(-1);
}

// CHANGE_LEVEL
void
change_level(int next_level)
{
	player->highest_level = next_level;

	switch(next_level)
	  {
		case(1):
		level_one();
		break;
		case(2):
		level_two();
		break;
		case(3):
		level_three();
		break;
		case(4):
		level_four();
		break;
		case(5):
		level_five();
		break;
		default:
		return;
	  }

	return;
}

// LEVEL_ONE
void
level_one(void)
{
	int		i, j;

	BG_COL = TEAL;
	BR_COL = BLACK;
	SN_COL = WHITE;
	HD_COL = DARK_RED;
	FD_COL = RED;

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col; ++j)
		  {
			if (i == 0 || i == ws.ws_row-1 || j == 0 || j == ws.ws_col-1 || j == ws.ws_col-2)
				matrix[i][j] = -1;
			else
				matrix[i][j] = 0;
		  }
	  }

	pthread_mutex_lock(&mutex);

	reset_right();
	reset_up();

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (matrix[i][j] == -1)
				draw_line_x(BR_COL, 1, 0);
			else
				draw_line_x(BG_COL, 1, 0);
		  }

		reset_right();
		up(1);
	  }

	reset_right();
	reset_up();

	/*clear_screen(BG_COL);
	draw_line_x(BR_COL, ws.ws_col-1, 0);
	draw_line_y(BR_COL, ws.ws_row, 1);
	draw_line_x(BR_COL, ws.ws_col-1, 1);
	draw_line_y(BR_COL, ws.ws_row, 0);*/

	write_stats();
	pthread_mutex_unlock(&mutex);

	reset_snake(&shead, &stail);
	USLEEP_TIME = get_default_sleep_time();

	return;
}

// LEVEL_TWO
void
level_two(void)
{
	int		i, j;
	int		row_4;
	int		row_34;
	int		col_3;
	int		col_23;

	BR_COL = BLACK;
	BG_COL = SALMON;
	SN_COL = PINK;
	HD_COL = DARK_GREY;
	FD_COL = PURPLE;

	row_4 = (ws.ws_row/4);
	row_34 = ((row_4 << 1)+row_4);
	col_3 = (ws.ws_col/3);
	col_23 = (col_3 << 1);

	pthread_mutex_lock(&mutex);

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (i == 0 || i == ws.ws_row-1) matrix[i][j] = -1;
			else if (j == 0 || j == ws.ws_col-2) matrix[i][j] = -1;
			else matrix[i][j] = 0;
		  }
	  }

	j = col_3;
	i = row_4;

	while (i < row_34)
		matrix[i++][j] = -1;

	j = col_23;
	i = row_4;

	while (i < row_34)
		matrix[i++][j] = -1;

	reset_right();
	reset_up();
	
	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (matrix[i][j] == -1)
				draw_line_x(BR_COL, 1, 0);
			else
				draw_line_x(BG_COL, 1, 0);
		  }

		reset_right();
		up(1);
	  }

	write_stats();
	pthread_mutex_unlock(&mutex);

	reset_snake(&shead, &stail);
	USLEEP_TIME -= 20000;

	return;
}

// LEVEL_THREE
void
level_three(void)
{
	int		i, j;

	BG_COL = ORANGE;
	BR_COL = BLACK;
	SN_COL = GREEN;
	HD_COL = DARK_RED;
	FD_COL = DARK_GREY;

	pthread_mutex_lock(&mutex);

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (i == 0)
				matrix[i][j] = -1;
			else if (i == ws.ws_row-1)
				matrix[i][j] = -1;
			else if (j == 0)
				matrix[i][j] = -1;
			else if (j == ws.ws_col-2)
				matrix[i][j] = -1;
			else if (((i == ws.ws_row-5) || (i == 5)) &&
				((j >= 10 && j < 30) || (j <= ws.ws_col-11 && j > ws.ws_col-11-20)))
				matrix[i][j] = -1;
			else if ((j == 20 || j == ws.ws_col-11-10) &&
				((i >= 5 && i < 10) || (i <= ws.ws_row-5 && i > ws.ws_row-10)))
				matrix[i][j] = -1;
			else if (((i == (ws.ws_row/2)-(ws.ws_row/8)) || (i == (ws.ws_row/2)+(ws.ws_row/8))) &&
				(j >= 30 && j < (ws.ws_col-11-20)))
				matrix[i][j] = -1;
			else
				matrix[i][j] = 0;
		  }
	  }

	clear_screen(BG_COL);
	reset_right();
	reset_up();

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (matrix[i][j] == -1)
				draw_line_x(BR_COL, 1, 0); 
			else
				draw_line_x(BG_COL, 1, 0);
		  }

		reset_right();
		up(1);
	  }


	write_stats();
	pthread_mutex_unlock(&mutex);

	reset_snake(&shead, &stail);
	USLEEP_TIME -= 20000;

	return;
}

// LEVEL_FOUR
void
level_four(void)
{
	int	i, j;

	BG_COL = TEAL;
	BR_COL = BLACK;
	SN_COL = DARK_RED;
	HD_COL = WHITE;
	FD_COL = SALMON;

	pthread_mutex_lock(&mutex);

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (i == 0)
				matrix[i][j] = -1;
			else if (i == ws.ws_row-1)
				matrix[i][j] = -1;
			else if (j == 0)
				matrix[i][j] = -1;
			else if (j == ws.ws_col-2)
				matrix[i][j] = -1;
			else if ((i == ws.ws_row-row_8 || i == row_8) &&
				((j >= col_8 && j < col_4) ||
				(j > ws.ws_col-col_4 && j <= ws.ws_col-col_8-1)))
					matrix[i][j] = -1;
			else if ((j == col_8 || j == ws.ws_col-col_8-1) &&
				((i >= row_8 && i < row_4) ||
				(i <= ws.ws_row-row_8 && i > ws.ws_row-row_4)))
					matrix[i][j] = -1;
			else if (i == row_8 &&
				(j >= (ws.ws_col/3) && j < ((ws.ws_col/3)*2)))
					matrix[i][j] = -1;
			else if ((j == (ws.ws_col/3) || j == ((ws.ws_col/3)*2)) &&
				(i >= row_8 && i < (row_8 + row_4)))
					matrix[i][j] = -1;
			else if (i == (row_4 + row_8) &&
				((j <= (ws.ws_col/3) && j > ((ws.ws_col/3)-(ws.ws_col/8))) ||
				(j >= ((ws.ws_col/3)*2) && j < (((ws.ws_col/3)*2)+(ws.ws_col/8)))))
					matrix[i][j] = -1;
			else if ((j == ((ws.ws_col/3)-(ws.ws_col/8)) || (j == (((ws.ws_col/3)*2)+(ws.ws_col/8)))) &&
				(i >= (row_8 + row_4) && i < (((ws.ws_row/2) + row_8))))
					matrix[i][j] = -1;
			else if (i == ((ws.ws_row/2) + row_8) &&
				((j >= ((ws.ws_col/3)-(ws.ws_col/8)) && j < ((ws.ws_col/2)-(ws.ws_col/8))) ||
				(j <= (((ws.ws_col/3)*2)+(ws.ws_col/8)) && j > ((ws.ws_col/2)+(ws.ws_col/8)))))
					matrix[i][j] = -1;
			else
				matrix[i][j] = 0;
		  }
	  }

	reset_right();
	reset_up();
	clear_screen(BG_COL);

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {	
			if (matrix[i][j] == -1)
				draw_line_x(BR_COL, 1, 0);
			else
				draw_line_x(BG_COL, 1, 0);
		  }

		reset_right();
		up(1);
	  }

	reset_cursor();

	write_stats();
	pthread_mutex_unlock(&mutex);

	reset_snake(&shead, &stail);
	USLEEP_TIME -= 20000;

	return;
}

// LEVEL_FIVE
void
level_five(void)
{
	int		i, j;
	
	BG_COL = SKY_BLUE;
	BR_COL = BLACK;
	SN_COL = LEMON;
	HD_COL = DARK_RED;
	FD_COL = GREEN;

	pthread_mutex_lock(&mutex);

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (i == 0 || i == (ws.ws_row-1) || j == 0 || j == (ws.ws_col-1))
				matrix[i][j] = -1;
			else if ((i == (row_2 - row_16) || i == (row_2 + row_16)) &&
				((j >= col_4 && j < (col_2 - col_8 + (col_16/2))) ||
				(j <= (col_max - col_4) && j > (col_2 + col_8 - (col_16/2)))))
					matrix[i][j] = -1;
			else if ((j == (col_2 - col_8 + (col_16/2)) || j == (col_2 + col_8 - (col_16/2))) &&
				((i >= (row_2 + row_16) && i < (row_2 + row_4)) ||
				(i <= (row_2 - row_16) && i > (row_2 - row_4))))
					matrix[i][j] = -1;
			else
				matrix[i][j] = 0;
		  }
	  }

	reset_right();
	reset_up();

	clear_screen(BG_COL);

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col-1; ++j)
		  {
			if (matrix[i][j] == -1)
				draw_line_x(BR_COL, 1, 0);
			else
				draw_line_x(BG_COL, 1, 0);
		  }

		reset_right();
		up(1);
	  }


	write_stats();
	pthread_mutex_unlock(&mutex);

	reset_snake(&shead, &stail);
	USLEEP_TIME -= 10000;

	
}

// RESET_SNAKE
void
reset_snake(Snake_Head *h, Snake_Tail *t)
{
	/* This function just frees all the links in the linked list */
	/* so that we are left with just one piece which we will try */
	/* to draw somewhere in the new level ("try", because the sn-*/
	/* may be quite long and there are obstacles in higher levels*/
	/* to consider */

	pthread_mutex_lock(&smutex);
	while (h->h->prev != NULL)
	  {
		h->h = h->h->prev;
		free(h->h->next);
		h->h->next = NULL;
		--(h->np);
	  }

	pthread_mutex_lock(&dir_mutex);
	DIRECTION = 0x6c;
	h->h->d = DIRECTION;
	pthread_mutex_unlock(&dir_mutex);

	//h->r = ((rand()%(maxr-1))+1);
	//h->u = ((rand()%(maxu-1))+1);

	/* always re-configure the screen matrix before this function so we can adjust these here */
	h->h->l = h->sl;
	h->r = default_r;
	h->u = default_u;

	pthread_mutex_unlock(&smutex);


	calibrate_snake_position(h, t);

	/*if (matrix[h->u][h->r] == -1 || (h->r + (h->sl - 1)) >= maxr)
	  {
		while (matrix[h->u][h->r] == -1 || (h->r + (h->sl - 1)) >= maxr)
			--(h->r);
		--(h->r);
	  }*/

	/*if (matrix[h->u][h->r] == -1 || h->r + h->sl >= maxr)
	//if (h->r + h->sl >= maxr)
	  {
		while (matrix[h->u][h->r] == -1 || h->r + h->sl >= maxr)
		//while (h->r + h->sl >= maxr)
		  {
			h->r = ((rand()%(maxr-1))+1);
			h->u = ((rand()%(maxu-1))+1);
		  }
		  //{ --(h->r);  --(t->r); }
	  }*/

	return;
}

//REDRAW
void
redraw_snake(Snake_Head *h, Snake_Tail *t)
{
	Snake_Piece		*p = NULL;

	go_to_tail(h, t);

	for (p = t->t; p != NULL; p = p->next)
	  {
		switch(p->d)
		  {
			case(0x75):
			if (p->prev != NULL)
				up(1);
			draw_line_y(SN_COL, p->l, 1);
			down(1);
			break;
			case(0x64):
			if (p->prev != NULL)
				down(1);
			draw_line_y(SN_COL, p->l, 0);
			up(1);
			break;
			case(0x6c):
			if (p->prev != NULL)
				left(1);
			draw_line_x(SN_COL, p->l, 1);
			right(1);
			break;
			case(0x72):
			if (p->prev != NULL)
				right(1);
			draw_line_x(SN_COL, p->l, 0);
			left(1);
			break;
		  }
	  }

	return;
}

// _PAUSE
void
_pause(void)
{
	char		*paused_text = "Game Paused";
	int		u, r;
	int		i;

	pthread_mutex_lock(&mutex);
	pthread_mutex_lock(&smutex);
	pthread_mutex_lock(&dir_mutex);
	pthread_mutex_lock(&sleep_mutex);

	save_screen_format(&shead, &stail);

	reset_right();
	reset_up();

	u = (ws.ws_row/2);
	r = ((ws.ws_col/2)-(strlen(paused_text)/2));
	up(u);
	right(r);

	for (i = 0; i < strlen(paused_text); ++i)
	  {
		if (matrix[u][r] == -1)
			printf("%s%s%c\e[m", BR_COL, TSKY_BLUE, paused_text[i]);
		else if (matrix[u][r] == 1)
			printf("%s%s%c\e[m", SN_COL, TSKY_BLUE, paused_text[i]);
		else if (matrix[u][r] == 2)
			printf("%s%s%c\e[m", FD_COL, TSKY_BLUE, paused_text[i]);
		else
			printf("%s%s%c\e[m", BG_COL, TSKY_BLUE, paused_text[i]);

		++r;
	  }

	reset_right();
	reset_up();

	return;
}

// UNPAUSE
void
unpause(void)
{
	int		i, j;

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col; ++j)
			if (matrix[i][j] == 2)
				matrix[i][j] = 0;
	  }

	restore_screen_format();

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col; ++j)
		  {
			if (matrix[i][j] == 1)
				matrix[i][j] = 0;
		  }
	  }

	reset_cursor();
	up(f.u);
	right(f.r);
	draw_line_x(FD_COL, 1, 0);
	reset_cursor();

	write_stats();

	pthread_mutex_unlock(&mutex);
	pthread_mutex_unlock(&smutex);
	pthread_mutex_unlock(&dir_mutex);
	pthread_mutex_unlock(&sleep_mutex);

}

static char
best_direction(int u, int d, int l, int r)
{
	if (u > d && u > l && u > r) return(0x75);
	else if (d > u && d > l && d > r) return(0x64);
	else if (l > u && l > d && l > r) return(0x6c);
	else if (r > u && r > d && r > l) return(0x72);
	else return(0x6c);
}

// CALIBRATE_SNAKE
void
calibrate_snake_position(Snake_Head *h, Snake_Tail *t)
{
	/* The snake might be very long and there may be many
	 * obstacles in the new level. So we have to find a
	 * suitable starting place for the snake, which may
	 * mean having to bend the snake around them. At this
	 * point, the matrix should have been reformatted
	 */

	int		head_r, head_u;
	int		save_r, save_u;
	int		slen;
	int		i, j;
	int		delta_d, delta_u, delta_r, delta_l;
	int		bad_l, bad_r, bad_u, bad_d;
	char		dir;

	pthread_mutex_lock(&mutex);
	pthread_mutex_lock(&smutex);

	/* The head square begins somewhere in the upper-right quadrant;
	 * If it happens to be on a -1 square, find the best direction
	 * to go in to find a 0 square
	 */
	head_r = default_r;
	head_u = default_u;

	reset_cursor();

	if (matrix[head_u][head_r] == -1)
	  {
		bad_r &= ~bad_r;
		bad_l &= ~bad_l;
		bad_u &= ~bad_u;
		bad_d &= ~bad_d;

		save_u = head_u;

		while (matrix[head_u][head_r] == -1)
			++head_u;

		bad_u = (head_u - save_u);

		head_u = save_u;

		while (matrix[head_u][head_r] == -1)
			--head_u;

		bad_d = (save_u - head_u);

		head_u = save_u;

		save_r = head_r;

		while (matrix[head_u][head_r] == -1)
			++head_r;

		bad_r = (head_r - save_r);

		head_r = save_r;

		while (matrix[head_u][head_r] == -1)
			--head_r;

		bad_l = (save_r - head_r);

		head_r = save_r;

		if (bad_l < bad_r && bad_l < bad_u && bad_l < bad_d)
			head_r -= bad_l;
		else if (bad_r < bad_l && bad_r < bad_u && bad_r < bad_d)
			head_r += bad_r;
		else if (bad_u < bad_l && bad_u < bad_r && bad_u < bad_d)
			head_u += bad_u;
		else
			head_u -= bad_d;
	  }

	/* Now that we've found a zero square on which to begin, find the
	 * best FORWARD direction to go in, move the head along half of
	 * the distance, and then proceed to draw the snake backwards in
	 * the screen matrix
	 */
	delta_d &= ~delta_d;
	delta_u &= ~delta_u;
	delta_r &= ~delta_r;
	delta_l &= ~delta_l;

	save_u = head_u;
	save_r = head_r;

	while (matrix[head_u][head_r] != -1 && head_r > 0)
	  { ++delta_l; --head_r; }

	head_r = save_r;

	while (matrix[head_u][head_r] != -1 && head_r < ws.ws_col-1)
	  { ++delta_r; ++head_r; }

	head_r = save_r;

	while (matrix[head_u][head_r] != -1 && head_u > 0)
	  { ++delta_d; --head_u; }

	head_u = save_u;

	while (matrix[head_u][head_r] != -1 && head_u < ws.ws_row)
	  { ++delta_u; ++head_u; }

	head_u = save_u;

	dir = best_direction(delta_u, delta_d, delta_l, delta_r);

	h->h->d = dir;

	switch(dir)
	  {
		case(0x75):
		head_u += (delta_u/3);
		dir = 0x64;
		break;
		case(0x64):
		head_u -= (delta_u/3);
		dir = 0x75;
		break;
		case(0x72):
		head_r += (delta_r/3);
		dir = 0x6c;
		break;
		case(0x6c):
		head_r -= (delta_l/3);
		dir = 0x72;
		break;
	  }

	/* Now we should have gotten the best FORWARD direction for the snake;
	 * we just have to then find an acceptable path backwards in the screen
	 * matrix representation
	 */
	reset_right();
	reset_up();

	h->u = head_u;
	h->r = head_r;

	switch(dir)
	  {
		case(0x72):
		t->r = (h->r + (h->sl - 1));
		t->u = h->u;
		break;
		case(0x6c):
		t->r = (h->r - (h->sl - 1));
		t->u = h->u;
		break;
		case(0x75):
		t->u = (h->u + (h->sl - 1));
		t->r = h->r;
		break;
		case(0x64):
		t->u = (h->u - (h->sl - 1));
		t->r = h->r;
		break;
	  }

	h->np = 1;

	matrix[head_u][head_r] = 1;
	t->t->l = slen = 1;

	debug("finding acceptable path in screen matrix");
	while (slen < h->sl)
	  {
		switch(dir)
		  {
			case(0x6c):
			while (matrix[head_u][(head_r-1)] != -1 && head_r > 0)
			  {
				matrix[head_u][--head_r] = 1;
				++slen; ++(t->t->l);
				if (slen >= h->sl) goto fini;
			  }

			delta_u &= ~delta_u;
			delta_d &= ~delta_d;

			save_u = head_u;

			while (matrix[head_u][head_r] != -1 && head_u < maxu)
				++head_u;

			delta_u = (head_u - save_u);

			head_u = save_u;

			while (matrix[head_u][head_r] != -1 && head_u > minu)
				--head_u;

			delta_d = (save_u - head_u);

			head_u = save_u;

			//--slen; --(t->t->l);
			pthread_mutex_unlock(&smutex);
			grow_tail(t);
			pthread_mutex_lock(&smutex);
			--(t->t->next->l);
			t->t->l = 1;
			++(h->np);

			if (delta_d > delta_u)
			  { dir = 0x64; t->t->d = 0x75; }
			else
			  { dir = 0x75; t->t->d = 0x64; }

			t->t->apex_u = head_u;
			t->t->apex_r = head_r;
			/*reset_right();
			reset_up();
			up(head_u);
			right(head_r);
			draw_line_x(PINK, 1, 0);*/
			break;
			case(0x72):
			while (matrix[head_u][(head_r+1)] != -1 && head_r < maxr)
			  {
				matrix[head_u][++head_r] = 1;
				++slen; ++(t->t->l);
				if (slen >= h->sl) goto fini;
			  }
			delta_u &= ~delta_u;
			delta_d &= ~delta_d;

			save_u = head_u;

			while (matrix[head_u][head_r] != -1 && head_u < maxu)
				++head_u;

			delta_u = (head_u - save_u);

			head_u = save_u;

			while (matrix[head_u][head_r] != -1 && head_u > minu)
				--head_u;

			delta_d = (save_u - head_u);

			head_u = save_u;


			//--slen; --(t->t->l);
			pthread_mutex_unlock(&smutex);
			grow_tail(t);
			pthread_mutex_lock(&smutex);
			--(t->t->next->l);
			t->t->l = 1;
			++(h->np);

			if (delta_d > delta_u)
			  { dir = 0x64; t->t->d = 0x75; }
			else
			  { dir = 0x75; t->t->d = 0x64; }

			t->t->apex_u = head_u;
			t->t->apex_r = head_r;
			/*reset_right();
			reset_up();
			up(head_u);
			right(head_r);
			draw_line_x(PINK, 1, 0);*/
			break;
			case(0x64):
			while (matrix[(head_u-1)][head_r] != -1 && head_u > minu)
			  {
				matrix[--head_u][head_r] = 1;
				++slen; ++(t->t->l);
				if (slen >= h->sl) goto fini;
			  }
			delta_l &= ~delta_l;
			delta_r &= ~delta_r;

			save_r = head_r;

			while (matrix[head_u][head_r] != -1 && head_r > minr)
				--head_r;

			delta_l = (save_r - head_r);

			head_r = save_r;

			while (matrix[head_u][head_r] != -1 && head_r < maxr)
				++head_r;

			delta_r = (head_r - save_r);

			head_r = save_r;

			//--slen; --(t->t->l);
			pthread_mutex_unlock(&smutex);
			grow_tail(t);
			pthread_mutex_lock(&smutex);
			--(t->t->next->l);
			t->t->l = 1;
			++(h->np);

			if (delta_r > delta_l)
			  { dir = 0x72; t->t->d = 0x6c; }
			else
			  { dir = 0x6c; t->t->d = 0x72; }

			t->t->apex_u = head_u;
			t->t->apex_r = head_r;
			/*reset_right();
			reset_up();
			up(head_u);
			right(head_r);
			draw_line_x(PINK, 1, 0);*/
			break;
			case(0x75):
			while (matrix[(head_u+1)][head_r] != -1 && head_u < maxu)
			  {
				matrix[++head_u][head_r] = 1;
				++slen; ++(t->t->l);
				if (slen >= h->sl) goto fini;
			  }
			delta_l &= ~delta_l;
			delta_r &= ~delta_r;

			save_r = head_r;

			while (matrix[head_u][head_r] != -1 && head_r > minr)
				--head_r;

			delta_l = (save_r - head_r);

			head_r = save_r;

			while (matrix[head_u][head_r] != -1 && head_r > maxr)
				++head_r;

			delta_r = (head_r - save_r);

			head_r = save_r;

			//--slen; --(t->t->l);
			pthread_mutex_unlock(&smutex);
			grow_tail(t);
			pthread_mutex_lock(&smutex);
			--(t->t->next->l);
			t->t->l = 1;
			++(h->np);

			if (delta_r > delta_l)
			  { dir = 0x72; t->t->d = 0x6c; }
			else
			  { dir = 0x6c; t->t->d = 0x72; }

			t->t->apex_u = head_u;
			t->t->apex_r = head_r;
			/*reset_right();
			reset_up();
			up(head_u);
			right(head_r);
			draw_line_x(PINK, 1, 0);*/
			break;
		  }
		//usleep(500000);
	  }

	fini:
	/* So now the path of the snake has been mapped into the matrix
	 * (backwards). All the snake pieces should have the correct
	 * direction and length with the coordinates of their apexes.
	 * Wherever we finished, that is the exact coordinates of the
	 * tail block.
	 */

	//--(t->t->l);

	t->r = head_r;
	t->u = head_u;

	//pthread_mutex_lock(&mutex);

	reset_right();
	reset_up();
	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col; ++j)
		  {
			if (matrix[i][j] == 1)
			  {
				up(i);
				right(j);
				draw_line_x(SN_COL, 1, 0);
				left(1);
				matrix[i][j] = 0;

				reset_cursor();
			  }
			else { continue; }
		  }
	  }

	reset_right();
	reset_up();

	up(h->u);
	right(h->r);

	pthread_mutex_unlock(&smutex);
	pthread_mutex_unlock(&mutex);
	return;
}

// GET_COLOUR
/*char *
get_colour(char *col)
{
	if (strcasecmp("blue", col) == 0)
		return(BLUE);
	else if (strcasecmp("yellow", col) == 0)
		return(YELLOW);
	else if (strcasecmp("orange", col) == 0)
		return(ORANGE);
	else if (strcasecmp("green", col) == 0)
		return(GREEN);
	else if (strcasecmp("black", col) == 0)
		return(BLACK);
	else if (strcasecmp("white", col) == 0)
		return(WHITE);
	else if (strcasecmp("olive", col) == 0)
		return(OLIVE);
	else if (strcasecmp("aqua", col) == 0)
		return(AQUA);
	else if (strcasecmp("salmon", col) == 0)
		return(SALMON);
	else if (strcasecmp("red", col) == 0)
		return(RED);
	else if (strcasecmp("darkred", col) == 0)
		return(DARK_RED);
	else if (strcasecmp("grey", col) == 0)
		return(GREY);
	else if (strcasecmp("darkgrey", col) == 0)
		return(DARK_GREY);
	else if (strcasecmp("lightgrey", col) == 0)
		return(LIGHT_GREY);
	else if (strcasecmp("pink", col) == 0)
		return(PINK);
	else if (strcasecmp("darkgreen", col) == 0)
		return(DARK_GREEN);
	else
		return(NULL);
}*/

// WRITE_HIGH_SCORES
int
write_high_scores(Player *list_head, Player *list_end)
{
	Player		*ptr = NULL;
	Player		player_storage;
	int		num;

	num &= ~num;
	clearerr(hs_fp);
	fseek(hs_fp, 0, SEEK_SET);

	for (ptr = list_head; ptr != NULL; ptr = ptr->next)
	  {
		memset(&player_storage, 0, sizeof(player_storage));
		memcpy(&player_storage, ptr, sizeof(*ptr));

		player_storage.next = NULL;
		player_storage.prev = NULL;

		fwrite(&player_storage, sizeof(player_storage), 1, hs_fp);
		if (ferror(hs_fp))
		  {
			fprintf(stderr, "write_hall_of_fame: error writing to FILE stream (%s)\n",
				strerror(errno));
			goto fail;
		  }

		++num;
		if (num == 100)
			break;
	  }

	return(0);

	fail:
	return(-1);
}

// WRITE_STATS
void
write_stats(void)
{
	memset(tmp, 0, MAXLINE);

	reset_right();
	reset_up();
	up(ws.ws_row-1);

	pthread_mutex_lock(&list_mutex);
	if (!player_list->first)
	  {
		sprintf(tmp, "Player %s | Score %06d | Level %d", player->name, player->score, level);
		clear_line(BR_COL);
		right((ws.ws_col/2)-(strlen(tmp)/2));

		printf("%s%sPlayer %s%s%s | Score %s%06d%s | Level %s%d\e[m",
			BR_COL,
			TWHITE,
			TGREEN,
			player->name,
			TWHITE,
			TSKY_BLUE,
			player->score,
			TWHITE,
			TSKY_BLUE,
			level);
	  }
	else
	  {
		if (player->score > player_list->first->score)
		  {
			sprintf(tmp, "Player %s | Score %06d | Level %d **New High Score**", player->name, player->score, level);
			clear_line(BR_COL);
			right((ws.ws_col/2)-(strlen(tmp)/2));

			printf("%s%sPlayer %s%s%s | Score %s%06d%s | Level %s%d %s**New High Score**\e[m",
				BR_COL,
				TWHITE,
				TGREEN,
				player->name,
				TWHITE,
				TRED,
				player->score,
				TWHITE,
				TSKY_BLUE,
				level,
				TORANGE);
		  }
		else
		  {
			sprintf(tmp, "Player %s | Score %06d | Level %d [All Time High: %s - %d]", player->name, player->score, level,
				player_list->first->name, player_list->first->score);
			clear_line(BR_COL);
			right((ws.ws_col/2)-(strlen(tmp)/2));

			printf("%s%sPlayer %s%s%s | Score %s%06d%s | Level %s%d%s [%sAll Time High: %s%s - %d%s]\e[m",
				BR_COL,
				TWHITE,
				TGREEN,
				player->name,
				TWHITE,
				TSKY_BLUE,
				player->score,
				TWHITE,
				TSKY_BLUE,
				level,
				TWHITE,
				TPINK,
				TORANGE,
				player_list->first->name,
				player_list->first->score,
				TWHITE);
		  }
	  }

	pthread_mutex_unlock(&list_mutex);

	reset_right();
	reset_up();

	up(shead.u);
	right(shead.r);

	return;
}

// TITLE_SCREEN
int
title_screen(void)
{
	int	i, j;
	int	game_title[8][39] = 
	  {
	    { 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0 },
	    { 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0 },
	    { 1, 1, 1, 0, 2, 2, 2, 2, 1, 1, 1, 0, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 0, 2, 2, 1, 1, 0, 2, 2, 2, 2 },
	    { 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 1, 1, 0, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0 },
	    { 2, 2, 2, 1, 1, 1, 0, 2, 1, 1, 0, 1, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 1, 0, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0 },
	    { 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 1, 1, 0, 2, 2, 1, 1, 0, 2, 2, 2, 2 },
	    { 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0 },
	    { 1, 1, 1, 1, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 0, 2, 1, 1, 1, 1, 1, 1, 0 }
	  };

	if (show_hall_of_fame(player_list->first, player_list->last) == -1)
		goto fail;

	pthread_mutex_lock(&mutex);

	reset_right();
	reset_up();

	up((ws.ws_row/2)+(ws.ws_row/4)+(ws.ws_row/8));
	center_x((39/2), 0);
	for (i = 0; i < 8; ++i)
	  {
		for (j = 0; j < 39; ++j)
		  {
			if (game_title[i][j] == 1)
				draw_line_x(RED, 1, 0);
			else if (game_title[i][j] == 2)
				draw_line_x(MENU_BG_COL, 1, 0);
			else
				draw_line_x(TEXT_SHADOW_COL, 1, 0);
		  }

		reset_right();
		down(1);
		center_x((39/2), 0);
	  }

	reset_right();
	reset_up();

	pthread_mutex_unlock(&mutex);

	return(0);

	fail:
	return(-1);
}

// GET_HIGH_SCORES
int
get_high_scores(Player **list_head, Player **list_end)
{
	Player		*node = NULL;
	Player		*nptr = NULL;
	Player		player_storage;
	struct stat	statb;
	size_t		bytes_so_far;
	int		ret;

	memset(&statb, 0, sizeof(statb));
	if (lstat(high_score_file, &statb) < 0)
	  { log_err("get_high_scores: lstat error"); goto fail; }

	bytes_so_far &= ~bytes_so_far;

	clearerr(hs_fp);
	fseek(hs_fp, 0, SEEK_SET);

	pthread_mutex_lock(&list_mutex);

	memset(&player_storage, 0, sizeof(player_storage));

	while (!feof(hs_fp))
	  {
		if ((ret = fread(&player_storage, sizeof(player_storage), 1, hs_fp)) < 0)
		  { log_err("get_high_scores: fread error"); goto fail; }

		if (!(node = new_player_node()))
		  { log_err("get_high_scores: new_player_node error"); goto fail; }

		memcpy(node, &player_storage, sizeof(player_storage));

		node->next = NULL;
		node->prev = NULL;

		if (*list_head == NULL)
		  {
			*list_head = node;
			*list_end = *list_head;
		  }
		else
		  {
			nptr = *list_head;

			while (nptr->next != NULL)
				nptr = nptr->next;

			nptr->next = node;
			node->prev = nptr;
			*list_end = node;
		  }

		bytes_so_far += sizeof(player_storage);

		debug("read %lu bytes from high scores file", bytes_so_far);
		if (bytes_so_far >= statb.st_size)
			break;
	  }

	for (nptr = *list_head; nptr != NULL; nptr = nptr->next)
		debug("nptr->name: %s", nptr->name);

	pthread_mutex_unlock(&list_mutex);

	return(0);

	fail:

	pthread_mutex_unlock(&list_mutex);
	return(-1);
}

Player *
new_player_node(void)
{
	static Player		*new_node = NULL;

	if (!(new_node = malloc(sizeof(Player))))
		return(NULL);

	memset(new_node, 0, sizeof(Player));
	new_node->prev = NULL;
	new_node->next = NULL;

	return(new_node);
}

// FREE_HIGH_SCORES
void
free_high_scores(Player **list_head, Player **list_end)
{
	Player		*last_node = NULL, *current_node = NULL;

	if (*list_head != NULL)
	  {
		current_node = *list_head;

		for (;;)
		  {
			last_node = current_node;
			current_node = current_node->next;
			free(last_node);
			if (current_node == NULL)
				break;
		  }
	  }

	*list_head = NULL;
	*list_end = NULL;
}

// SHOW_HALL_OF_FAME
int
show_hall_of_fame(Player *list_head, Player *list_end)
{
	Player		*ptr = NULL;
	struct tm	TIME;
	char		time_string[32];
	int		cnt;
	char		*arrows = ">>>>>>>>>>>>>>>>>>>>";

	cnt &= ~cnt;

	pthread_mutex_lock(&mutex);

	reset_right();
	reset_up();

	up((ws.ws_row/2)-(ws.ws_row/8));
	right((ws.ws_col/2)-(strlen("Hall of Fame")/2));
	printf("%s%sHall of Fame\e[m", MENU_BG_COL, TORANGE);
	reset_right();
	down(2);
	right((ws.ws_col/2)-(76/2));

	pthread_mutex_lock(&list_mutex);
	if (!list_head)
		printf("%s%s     No hall of famers! :(\e[m\n", MENU_BG_COL, TSKY_BLUE);

	else
	  {
		for (ptr = list_head; ptr != NULL; ptr = ptr->next)
	  	  {
			memset(&TIME, 0, sizeof(TIME));
			if (gmtime_r(&ptr->when, &TIME) == NULL)
			  { log_err("show_hall_of_fame: gmtime_r error"); goto fail; }

			if (strftime(time_string, 32, "%02d/%m/%Y", &TIME) < 0)
			  { log_err("show_hall_of_fame: strftime error"); goto fail; }

			printf("%s%s%s#%d %s%s %*.*s%s Score: %s%6d%s | Food: %s%4d%s | Level: %s%d %s(%s%s%s)\e[m",
				MENU_BG_COL, TPINK,
				(ptr->position<10?" ":""),
				ptr->position,
				TTEAL, ptr->name,
				(int)(20-strlen(ptr->name)),
				(int)(20-strlen(ptr->name)),
				arrows, TWHITE,
				TBLUE, ptr->score, TWHITE,
				TBLUE, ptr->num_eaten, TWHITE,
				TBLUE, ptr->highest_level,
				TWHITE, TRED, time_string, TWHITE);

			reset_right();
			down(1);
			right((ws.ws_col/2)-(76/2));
			++cnt;
			if (cnt == 10) // only show 10 hall of famers!
				break;
		  }
	  }

	pthread_mutex_unlock(&list_mutex);

	reset_right();
	reset_up();

	pthread_mutex_unlock(&mutex);
	return(0);

	fail:
	pthread_mutex_unlock(&list_mutex);
	pthread_mutex_unlock(&mutex);
	return(-1);
}

// CHECK_CURRENT_PLAYER_SCORE
int
check_current_player_score(Player *current_player, Player *list_head, Player *list_end)
{
	Player			*lptr = NULL;
	int			inserted;

	inserted &= ~inserted;
	time(&current_player->when);
	current_player->position = 1;

	pthread_mutex_lock(&list_mutex);

	for (lptr = list_head; lptr != NULL; lptr = lptr->next)
	  {
		if (strcmp(current_player->name, lptr->name) == 0)
		  {
			if (current_player->score > lptr->score)
			  {
				lptr->score = current_player->score;
				lptr->position = current_player->position;
				lptr->highest_level = level;
				time(&lptr->when);
				lptr->num_eaten = current_player->num_eaten;
				BEAT_OWN_SCORE = 1;
				inserted = 1;
				break;
			  }
			else
			  {
				/* we didn't beat our score, so just stop now */
				break;
			  }
		  }

		if (current_player->score > lptr->score ||
			current_player->score == lptr->score)
		  {
			if (current_player->position == 1)
				NEW_BEST_PLAYER = 1;

			lptr->prev->next = current_player;
			current_player->prev = lptr->prev;
			current_player->next = lptr;
			lptr->prev = current_player;

			if (current_player->score == lptr->score)
			  {
				while (lptr->score == current_player->score && lptr->next != NULL)
					lptr = lptr->next;
			  }

			if (current_player->score == lptr->score && lptr->next == NULL)
			  { inserted = 1; break; }

			while (lptr != NULL)
			  {
				++(lptr->position);
				lptr = lptr->next;
			  }

			inserted = 1;
			break;
		  }
		else
		  {
			++(current_player->position);
			continue;
		  }
	  }

	if (!list_head)
		 list_head = current_player;

	if (!inserted)
	  {
		list_end->next = current_player;
		current_player->prev = list_end;
		list_end = current_player;
	  }

	pthread_mutex_unlock(&list_mutex);
	return(0);
}

// LOG_ERR
void
log_err(char *fmt, ...)
{
	va_list		args;
	char		*_tmp = NULL;

	_tmp = calloc(MAXLINE, 1);
	memset(_tmp, 0, MAXLINE);

	va_start(args, fmt);
	vsprintf(_tmp, fmt, args);
	va_end(args);

	fprintf(stderr, "%s (%s)\n", _tmp, strerror(errno));

	if (_tmp != NULL) { free(_tmp); _tmp = NULL; }
	return;
}

void
debug(char *fmt, ...)
{
	char	*_tmp = NULL;
	va_list	args;

	if (DEBUG)
	  {
		_tmp = calloc(MAXLINE, 1);
		memset(_tmp, 0, MAXLINE);
		va_start(args, fmt);
		vsprintf(_tmp, fmt, args);
		va_end(args);
		strip_crnl(_tmp);
		fprintf(stderr, "\e[48;5;0m\e[38;5;9m-+[debug]+- %s\e[m\n", _tmp);
		if (_tmp != NULL) { free(_tmp); _tmp = NULL; }
	  }
}

// GET_DEFAULT_SLEEP_TIME
int
get_default_sleep_time(void)
{
	double	seconds = 0.0;
	double	squares_per_second = 0.0;
	int	useconds;

	seconds = 0.09;
	squares_per_second = (((double)1)/seconds);

	while ((((double)ws.ws_col)/squares_per_second) > FOOD_REFRESH_TIME)
	  {
		seconds -= 0.01;
		squares_per_second = (((double)1)/seconds);
	  }

	useconds = (int)(seconds * 1000000);
	return(useconds);
}

// SAVE_SCREEN_FORMAT
void
save_screen_format(Snake_Head *h, Snake_Tail *t)
{
	Snake_Piece		*p = NULL;
	int			l;
	int			r, u;

	r = t->r;
	u = t->u;

	for (p = t->t; p != NULL; p = p->next)
	  {
		switch(p->d)
		  {
			case(0x75):
			if (p != t->t)
				++u;
			l &= ~l;
			while (l < p->l)
			  {
				matrix[u][r] = 1;
				++l;
				if (l < p->l) ++u;
			  }
			break;
			case(0x64):
			if (p != t->t)
				--u;
			l &= ~l;
			while (l < p->l)
			  {
				matrix[u][r] = 1;
				++l;
				if (l < p->l) --u;
			  }
			break;
			case(0x6c):
			if (p != t->t)
				--r;
			l &= ~l;
			while (l < p->l)
			  {
				matrix[u][r] = 1;
				++l;
				if (l < p->l) --r;
			  }
			break;
			case(0x72):
			if (p != t->t)
				++r;
			l &= ~l;
			while (l < p->l)
			  {
				matrix[u][r] = 1;
				++l;
				if (l < p->l) ++r;
			  }
			break;
		  }	
	  }

	matrix[f.u][f.r] = 2;

	return;
}

// RESTORE_SCREEN_FORMAT
void
restore_screen_format(void)
{
	int		i, j;

	reset_right();
	reset_up();

	for (i = 0; i < ws.ws_row; ++i)
	  {
		for (j = 0; j < ws.ws_col; ++j)
		  {
			if (matrix[i][j] == -1)
				draw_line_x(BR_COL, 1, 0);
			else if (matrix[i][j] == 1)
				draw_line_x(SN_COL, 1, 0);
			else if (matrix[i][j] == 2)
				draw_line_x(FD_COL, 1, 0);
			else
				draw_line_x(BG_COL, 1, 0);
		  }

		reset_right();
		up(1);
	  }

	return;
}

void
strip_crnl(char *line)
{
	char		*p = NULL;
	size_t		l;

	l = strlen(line);

	p = (line + (l - 1));
	if (*p != 0x0d && *p != 0x0a) return;

	while ((*p == 0x0d || *p == 0x0a) && p > (line + 1)) --p;

	++p;

	*p = 0;

	return;
}
