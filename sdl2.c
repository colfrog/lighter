#include <SDL2/SDL.h>

#include <sys/time.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <errno.h>
#include <err.h>

#define X 2000
#define Y 2000
#define HOUR 3600

typedef struct rgb {
	unsigned short r, g, b;
} rgb;

typedef struct rgb_setting {
	time_t starting_time, duration;
	rgb rgb_start, rgb_end;
	rgb *rgb;
	struct rgb_setting *next;
} rgb_setting;

// 6 to 9
static rgb_setting dusk = {
	.starting_time = 0, .duration = 14,
	.rgb_start = {0x00, 0xff, 0x00},
	.rgb_end = {0x00, 0x00, 0xff},
	.rgb = NULL,
	.next = NULL
};

// 19 to 22
static rgb_setting dawn = {
	.starting_time = 0, .duration = 14,
	.rgb_start = {0xff, 0x00, 0x00},
	.rgb_end = {0xff, 0xff, 0x00},
	.rgb = NULL,
	.next = NULL
};

// 9 to 19
static rgb_setting day = {
	.starting_time = 0, .duration = 14,
	.rgb_start = {0xff, 0xff, 0x00},
	.rgb_end = {0x00, 0xff, 0x00},
	.rgb = NULL,
	.next = NULL
};

// 22 to 6
static rgb_setting night = {
	.starting_time = 0, .duration = 14,
	.rgb_start = {0x00, 0x00, 0xff},
	.rgb_end = {0xff, 0x00, 0x00},
	.rgb = NULL,
	.next = NULL
};

static rgb_setting transition_rgb_setting;
static rgb_setting *current_rgb_setting;

static bool running = true;
static SDL_Window *win;
static SDL_Renderer *ren;

static pthread_t thread;
static pthread_mutex_t mtx;

void set_rgb_setting(rgb_setting *setting) {
	time_t t, d = 5;

	pthread_mutex_lock(&mtx);
	time(&t);

	if (current_rgb_setting == NULL) {
		setting->rgb = NULL;
		current_rgb_setting = setting;
		setting->starting_time = t;
		return;
	}

	transition_rgb_setting.duration = d;
	transition_rgb_setting.starting_time = t;
	setting->starting_time = t + d;

	transition_rgb_setting.rgb_start = *(current_rgb_setting->rgb);
	transition_rgb_setting.rgb_end = setting->rgb_start;

	transition_rgb_setting.next = setting;
	current_rgb_setting = &transition_rgb_setting;
	pthread_mutex_unlock(&mtx);
}

void *sdl_loop(void *np) {
	SDL_Event event;

	while (running) {
		SDL_PollEvent(&event);
		if (event.type == SDL_QUIT)
			running = false;
		if (event.type == SDL_KEYDOWN) {
			switch (event.key.keysym.sym) {
			case SDLK_q:
				running = false;
				break;

			case SDLK_a:
				set_rgb_setting(&dawn);
				break;
			case SDLK_s:
				set_rgb_setting(&day);
				break;
			case SDLK_d:
				set_rgb_setting(&dusk);
				break;
			case SDLK_f:
				set_rgb_setting(&night);
				break;

			default:
				break;
			}
		}

	}

	SDL_DestroyRenderer(ren);
	SDL_Quit();
}

int main() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
		err(errno, SDL_GetError());

	win = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, X, Y, SDL_WINDOW_SHOWN);
	ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

	pthread_create(&thread, NULL, sdl_loop, NULL);

	rgb_setting *cur;
	double dr, dg, db, dt;
	time_t end;
	struct timeval t;
	rgb current_rgb;

	current_rgb_setting = cur = &dawn;
	time(&cur->starting_time);
	dawn.next = &day;
	day.next = &dusk;
	dusk.next = &night;
	night.next = &dawn;

	SDL_Rect rect = {0, 0, X, Y};
	SDL_SetRenderTarget(ren, NULL);
	SDL_SetRenderDrawColor(ren, 0, 0, 0, 0);
	SDL_RenderFillRect(ren, &rect);
	while (running) {
		pthread_mutex_lock(&mtx);
		cur = current_rgb_setting;
		end = cur->starting_time + cur->duration;
		gettimeofday(&t, NULL);
		if (t.tv_sec > end && cur->next != NULL) {
			current_rgb_setting = cur = cur->next;
			cur->starting_time = t.tv_sec;
		}

		cur->rgb = &current_rgb;
		dr = cur->rgb_end.r - cur->rgb_start.r;
		dg = cur->rgb_end.g - cur->rgb_start.g;
		db = cur->rgb_end.b - cur->rgb_start.b;
		dt = (1e6*end - (1e6*t.tv_sec + t.tv_usec))/(1e6*cur->duration);

		// printf("%g %g %g %g\n", dt, dr, dg, db);

		if (cur->starting_time == t.tv_sec)
			dt = 1;
		else if (dt < 0 || t.tv_sec >= end)
			dt = 0;

		current_rgb.r = cur->rgb_end.r - dr*dt;
		current_rgb.g = cur->rgb_end.g - dg*dt;
		current_rgb.b = cur->rgb_end.b - db*dt;
		pthread_mutex_unlock(&mtx);

		SDL_SetRenderDrawColor(ren, current_rgb.r, current_rgb.g, current_rgb.b, 0xff);
		SDL_RenderFillRect(ren, &rect);
		SDL_RenderPresent(ren);

		SDL_Delay(10);
	}
}
