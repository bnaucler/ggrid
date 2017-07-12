#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define TITLE "ggrid test 1"
#define MAPFILE "map.txt"

#define BLOCKSZ 16
#define BLOCKNUM 32

typedef struct Block {
	int xind, yind;
	SDL_Rect *rect;
	SDL_Texture *tex;
	int wall;
} Block;

typedef struct Tex {
	SDL_Texture *brick;
	SDL_Texture *tile;
} Tex;

typedef struct Movement {
	int move;
	int tl;
	int tr;
	int tu;
	int td;
} Movement;

typedef struct Object {
	int xind, yind;
	SDL_Rect *rect;
	SDL_Texture *tex;
	Movement *mvmt;
} Object;

static void die(const char *err, int ret) {

	if(err[0]) fprintf(stderr, "Error: %s\n", err);

	SDL_Quit();
	exit(ret);
}

static SDL_Texture *selecttexture(const int x,
	const int y, const char mobj, Tex *textures) {

	SDL_Texture *tex;

	switch(mobj) {

		case ' ':
		case 'p':
			tex = textures->tile;
			break;

		case 'x':
			tex = textures->brick;
			break;

		default:
			return NULL;
	}

	if(!tex) return NULL;

	return tex;
}

static int iswall(Block *block, const char mobj) {

	if(mobj == 'x') return 1;
	else return 0;
}

static Block *initblock(const char mobj, Tex *textures, const int x,
	const int y, const int sz) {

	Block *block = calloc(1, sizeof(Block));
	block->rect = calloc(1, sizeof(SDL_Rect));

	block->xind = x;
	block->yind = y;

	block->rect->x = x * sz;
	block->rect->y = y * sz;
	block->rect->w = sz;
	block->rect->h = sz;

	block->wall = iswall(block, mobj);
	block->tex = selecttexture(x, y, mobj, textures);

	return block;
}

static Block **initline(char *mline, Tex *textures, const int sz,
	const int y, const int cols) {

	Block **line = calloc(cols, sizeof(Block*));

	int a = 0, x = 0;

	for(a = 0; a < cols; a++) {
		line[a] = initblock(mline[a], textures, x, y, sz);
		x++;
	}

	return line;
}

static Block ***initgrid(char **map, Tex *textures, const int sz,
	const int lines, const int cols) {

	Block ***grid = calloc(lines, sizeof(Block**));

	int a = 0, y = 0;

	for(a = 0; a < lines; a++) {
		grid[a] = initline(map[a], textures, sz, y, cols);
		y++;
	}

	return grid;
}

static SDL_Texture *loadtex(SDL_Renderer *rend, const char *fname) { 

	SDL_Surface *surf = IMG_Load(fname);
	if(!surf) return NULL;

	SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);

	SDL_FreeSurface(surf);
	return tex;
}

static Tex *loadalltex(SDL_Renderer *rend) {

	Tex *textures = calloc(1, sizeof(Tex));

	textures->brick = loadtex(rend, "brick.png");
	textures->tile = loadtex(rend, "tile.png");
	if(!textures->brick || !textures->tile) return NULL;

	return textures;
}

// Process incoming events
static int readevent(SDL_Event *event, Object *player) {

	while (SDL_PollEvent(event)) {
		switch (event->type) {

		case SDL_QUIT:
			return 1;

		case SDL_KEYDOWN:
			switch (event->key.keysym.scancode) {
				case SDL_SCANCODE_Q:
				case SDL_SCANCODE_ESCAPE:
					return 1;

				case SDL_SCANCODE_W:
				case SDL_SCANCODE_UP:
					player->mvmt->tu = 1;
					break;

				case SDL_SCANCODE_S:
				case SDL_SCANCODE_DOWN:
					player->mvmt->td = 1;
					break;

				case SDL_SCANCODE_A:
				case SDL_SCANCODE_LEFT:
					player->mvmt->tl = 1;
					break;

				case SDL_SCANCODE_D:
				case SDL_SCANCODE_RIGHT:
					player->mvmt->tr = 1;
					break;

				default:
					break;
			}
			break;

		case SDL_KEYUP:
			switch (event->key.keysym.scancode) {

				case SDL_SCANCODE_W:
				case SDL_SCANCODE_UP:
					player->mvmt->tu = 0;
					break;

				case SDL_SCANCODE_S:
				case SDL_SCANCODE_DOWN:
					player->mvmt->td = 0;
					break;

				case SDL_SCANCODE_A:
				case SDL_SCANCODE_LEFT:
					player->mvmt->tl = 0;
					break;

				case SDL_SCANCODE_D:
				case SDL_SCANCODE_RIGHT:
					player->mvmt->tr = 0;
					break;

				default:
					break;
			}
			break;
		}
	}

	return 0;
}

void printmap(const char **map) {

	do puts(*map); while(*++map);
}

static void readmap(FILE *mfp, char **map, const int lines, const int cols) {

	unsigned int a = 0;

	for(a = 0; a < lines; a++) {
		map[a] = calloc(cols + 2, sizeof(map));
		fgets(map[a], cols + 1, mfp);
		fseek(mfp, 1, SEEK_CUR);
		map[a][cols] = 0;
	}
}

static char **mkmap(const char *fname, const int lines, const int cols) {

	char **map = calloc(lines, sizeof(*map));

	FILE *mfp = fopen(fname, "r");
	if(!mfp) die("Could not open map file", 3);

	readmap(mfp, map, BLOCKNUM, BLOCKNUM);
	fclose(mfp);

	return map;
}

// Assuming only one occurance of key
static int getpos(char **map, const char key, Object *obj) {

	unsigned int y = 0, x = 0;

	while(map[y][0]) {
		while(map[y][x]) {
			if(map[y][x] == key) {
				obj->xind = x;
				obj->yind = y;
				return 0;	
			}
			x++;
		}
		x = 0;
		y++;
	}

	return 1;
}

static Object *mkobj(SDL_Renderer *rend, char **map, const char key,
	const char *texfile, const int w, const int h) {

	Object *obj = calloc(1, sizeof(Object));
	obj->rect = calloc(1, sizeof(SDL_Rect));
	obj->mvmt = calloc(1, sizeof(Movement));

	if(getpos(map, key, obj)) return NULL;

	obj->rect->w = w;
	obj->rect->h = h;

	if(texfile[0]) obj->tex = loadtex(rend, texfile);

	return obj;
}

static void drawline(SDL_Renderer *rend, Block **line, const int cols) {

	unsigned int x = 0;

	for(x = 0; x < cols; x++) {
		SDL_RenderCopyEx(rend, line[x]->tex, line[x]->rect, line[x]->rect,
			0, NULL, SDL_FLIP_NONE);
	}
}

static void drawgrid(SDL_Renderer *rend, Block ***grid, const int lines,
	const int cols) {

	unsigned int y = 0;

	for(y = 0; y < lines; y++) drawline(rend, grid[y], cols);
}

void drawobj(SDL_Renderer *rend, Object *obj) {

	if(obj->tex) {
		SDL_RenderCopyEx(rend, obj->tex, NULL, obj->rect,
			0, NULL, SDL_FLIP_NONE);

	} else {
		SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
		SDL_RenderFillRect(rend, obj->rect);
	}
}

void draw(SDL_Renderer *rend, Object *player, Block ***grid) {

	drawgrid(rend, grid, BLOCKNUM, BLOCKNUM);
	drawobj(rend, player);

	SDL_RenderPresent(rend);
}

static void chcol(Object *player, Block ***grid) {

	// Border collision
	if(player->xind <= 0) player->xind = 0;
	if(player->xind >= BLOCKNUM) player->xind = BLOCKNUM - 1; 

	if(player->yind <= 0) player->yind = 0;
	if(player->yind >= BLOCKNUM) player->yind = BLOCKNUM - 1; 

	// Grid collision
	if(grid[player->yind][player->xind]->wall) {
		if(player->mvmt->tu) player->yind++;
		if(player->mvmt->td) player->yind--;

		if(player->mvmt->tl) player->xind++;
		if(player->mvmt->tr) player->xind--;
	}
}

static void calrect(Object *obj, const int sz) {

	obj->rect->x = obj->xind * sz;
	obj->rect->y = obj->yind * sz;
}

static void mvplayer(Object *player) {

	if(player->mvmt->tl) player->xind--;
	if(player->mvmt->tr) player->xind++;

	if(player->mvmt->tu) player->yind--;
	if(player->mvmt->td) player->yind++;
}

static void freeblock(Block *block) {

	SDL_DestroyTexture(block->tex);
	free(block->rect);
	free(block);
}

static void freegrid(Block ***block, const int ysz, const int xsz) {

	unsigned int y = 0, x = 0;

	for(y = 0; y < ysz; y++) {
		for(x = 0; x < xsz; x++) freeblock(block[y][x]);
	}
}

static void freeobj(Object *obj) {

	free(obj->rect);
	free(obj->mvmt);
	free(obj);
}

int main(void) {

	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window *win = SDL_CreateWindow(TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (BLOCKSZ * BLOCKNUM), (BLOCKSZ * BLOCKNUM), 0);

	SDL_Renderer *rend = SDL_CreateRenderer(win, -1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	if(!win || !rend) return 2;

	SDL_Event *event = calloc(1, sizeof(SDL_Event));
	char **map = mkmap(MAPFILE, BLOCKNUM, BLOCKNUM);
	Tex *textures = loadalltex(rend);
	Block ***grid = initgrid(map, textures, BLOCKSZ, BLOCKNUM, BLOCKNUM);
	Object *player = mkobj(rend, map, 'p', "ball.png", BLOCKSZ, BLOCKSZ);

	while(!readevent(event, player)) {
		mvplayer(player);
		chcol(player, grid);
		calrect(player, BLOCKSZ);
		draw(rend, player, grid);
		SDL_Delay(50);
	}

	freeobj(player);
	freegrid(grid, BLOCKNUM, BLOCKNUM);

	SDL_DestroyWindow(win);
	SDL_DestroyRenderer(rend);
	die("", 0);
}
