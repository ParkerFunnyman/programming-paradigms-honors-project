//Name: Parker Carroll
//Date: April 27, 2026
//Description: The main goal of this Honors Project is to replicate
//Assignment 4 on the GBA, which uses C, which is a lower-leve;
//language and is notoriously NOT object-oriented
#include <gba_console.h>
#include <gba_video.h>
#include <gba_interrupt.h>
#include <gba_systemcalls.h>
#include <gba_input.h>
#include <gba_dma.h>
#include <gba_sprites.h>
#include <stdio.h>
#include <stdlib.h>

// Include user files here
#include "Tiles_tile_pal.h"
#include "MrsPacmanSpriteSheet_tile_pal.h"
// End user file includes

// Add user macros here
#define SPRT ((volatile Sprite *)0x07000000)
// End user macros

// Add variables here
const int bg0_base = 8;
const int bg1_base = 16;

extern const unsigned char GroundMap1_tilemap[];
extern const unsigned int GroundMap1_len;

extern const unsigned char ForegroundMap1_tilemap[];
extern const unsigned int ForegroundMap1_len;

int fruitsEaten = 0; // Global variable to keep track of how many fruits eaten
int ghostsEaten = 0; // Ditto, but counts number of ghosts
int camY = 0;		 // Global variable that acts as a scroll mechanic
// End variables here

// Add user functions here

inline void setVideoSettings()
{
	// Set video settings (find in cowbite or gba_video.h)
	REG_DISPCNT =
		MODE_0 |	 // Enable mode 0 (regular tiled backgrounds)
		OBJ_ENABLE | // Enable sprites (often referred to as "objects")
		OBJ_1D_MAP | // Set sprites to be drawn with sequentially store tiles
		BG0_ENABLE | // Enable background 0
		BG1_ENABLE;	 // Enable background 1
	return;
}

// Set settings for backgrounds 1 and 0
inline void setBGSettings()
{ // Share the same tile set
	// Change priorities so bg 1 is in front of bg 0
	REG_BG0CNT = BG_256_COLOR | BG_TILE_BASE(0) | TEXTBG_SIZE_512x512 |
				 BG_PRIORITY(1) | BG_MAP_BASE(bg0_base);
	REG_BG1CNT = BG_256_COLOR | BG_TILE_BASE(0) | TEXTBG_SIZE_512x512 |
				 BG_PRIORITY(0) | BG_MAP_BASE(bg1_base);
	// Use 256-color palette //Set bg to 512x512 pixels
	// Set the starting address for each tilemap
	return;
}

// Load 256-color palette for bg
static inline void loadBGPalette(const u16 *palette)
{
	dmaCopy(palette, BG_PALETTE, 256); // number_of_16_bit_words = 256
}
// Load set of tiles for bg
static inline void loadBGTileSet(const u8 *tileset)
{
	dmaCopy(tileset, TILE_BASE_ADR(0), 16384);
}

// Load ground tile map
static inline void loadGroundTileMap()
{
	CpuFastSet(GroundMap1_tilemap, MAP_BASE_ADR(bg0_base), GroundMap1_len);
}
// Load collidable tile map
static inline void loadForegroundMap()
{
	CpuFastSet(ForegroundMap1_tilemap, MAP_BASE_ADR(bg1_base), ForegroundMap1_len);
}

// Load sprite palette
static inline void loadSpritePalette(const u16 *palette)
{
	dmaCopy(palette, SPRITE_PALETTE, 256); // number_of_16_bit_words = 256
}
// Load sprite tiles
static inline void loadSpriteTileSet(const u8 *tileset)
{
	dmaCopy(tileset, SPRITE_GFX, 16384);
}

u16 getForegroundTile(u16 x, u16 y)
{
	int xtile = x >> 3;
	int ytile = y >> 3;
	if (xtile < 32)
	{
		if (ytile < 32) // upper left quadrant (0)
		{
			// y * 32
			return ((vu16 *)MAP_BASE_ADR(bg1_base))[(ytile << 5) + xtile];
		}
		else // lower left quadrant (2)
		{	 //(y%32)*32
			return ((vu16 *)MAP_BASE_ADR(bg1_base))[((ytile & 31) << 5) + xtile +
													2 * 32 * 32];
		}
	}
	else
	{
		if (ytile < 32) // upper right quadrant (1)
		{
			return ((vu16 *)MAP_BASE_ADR(bg1_base))[(ytile << 5) + (xtile & 31) +
													32 * 32];
		}
		else // lower right quadrant (3)
		{
			return ((vu16 *)MAP_BASE_ADR(bg1_base))[((ytile & 31) << 5) + (xtile & 31) +
													3 * 32 * 32];
		}
	}
	return 0;
}

// These four functions return true if a certain button is pressed
bool isUp()
{
	return !(REG_KEYINPUT & KEY_UP);
}
bool isDown()
{
	return !(REG_KEYINPUT & KEY_DOWN);
}

bool isLeft()
{
	return !(REG_KEYINPUT & KEY_LEFT);
}

bool isRight()
{
	return !(REG_KEYINPUT & KEY_RIGHT);
}

// These next three functions set up the sprites for MsPacman, Ghosts, and Fruits
static inline void setupMsPacmanSprite()
{
	SPRT[0].ColorMode = 1;		 // Set it to 256 color mode
	SPRT[0].Priority = 0;		 // Make him below foreground level
	SPRT[0].Shape = SQUARE;		 // All link graphics are 16x16 pixel squares
	SPRT[0].Size = Sprite_16x16; // 16x16 pixel squares
	SPRT[0].Character = 4 << 1;	 // Tiles are "twice as large" in 256-color mode

	SPRT[0].X = 0;
	SPRT[0].Y = 0;
}

static inline void setupGhost(int g)
{
	if (g <= 0)
	{
		return;
	}

	SPRT[g].ColorMode = 1;		 // Set it to 256 color mode
	SPRT[g].Priority = 0;		 // Make him below foreground level
	SPRT[g].Shape = SQUARE;		 // All link graphics are 16x16 pixel squares
	SPRT[g].Size = Sprite_16x16; // 16x16 pixel squares
	SPRT[g].Character = 52 << 1; // Tiles are "twice as large" in 256-color mode

	SPRT[g].X = 0;
	SPRT[g].Y = 0;
}

static inline void setUpFruit(int f)
{
	// SPRT[f].PaletteIndex = 1;
	SPRT[f].ColorMode = 1;		  // Set it to 256 color mode
	SPRT[f].Priority = 0;		  // Make him below foreground level
	SPRT[f].Shape = SQUARE;		  // All link graphics are 16x16 pixel squares
	SPRT[f].Size = Sprite_16x16;  // 16x16 pixel squares
	SPRT[f].Character = 116 << 1; // Tiles are "twice as large" in 256-color mode

	SPRT[f].X = 0;
	SPRT[f].Y = 0;
}

// Sets up scorekeeper sprites
static inline void setupIconSprites()
{
	for (int i = 9; i <= 12; i++)
	{
		SPRT[i].ColorMode = 1;
		SPRT[i].Shape = 3;
		SPRT[i].Size = Sprite_16x16;
		SPRT[i].Priority = 0;
		SPRT[i].Character = 52 << 1;
		SPRT[i].X = 4 + (i - 9) * 18; // left side
		SPRT[i].Y = 4;
	}
	for (int i = 13; i <= 16; i++)
	{
		SPRT[i].ColorMode = 1;
		SPRT[i].Shape = 3;
		SPRT[i].Size = Sprite_16x16;
		SPRT[i].Priority = 0;
		SPRT[i].Character = 116 << 1;
		SPRT[i].X = 240 - 20 - (i - 13) * 18; // right side
		SPRT[i].Y = 4;
	}
}

// Defines a structure for ghost objects
struct Ghost
{
	u16 x;
	u16 y;
	const short spd;
	int direction;
	int frame;
	int animCount;
	int dyingCount;
	bool dying;
	const u16 sprites[4][2];
	const u16 deadsprites[2][2];
};

// Instantiates an array of 4 ghost structs
struct Ghost ghosts[4] = {{.x = 20, .y = 20, .spd = 1, .direction = 0, .frame = 0, .animCount = 0, .dyingCount = 0, .dying = false, .sprites = {{52 << 1, 56 << 1}, {60 << 1, 64 << 1}, {68 << 1, 72 << 1}, {76 << 1, 80 << 1}}, .deadsprites = {{84 << 1, 88 << 1}, {92 << 1, 96 << 1}}},

						  {.x = 150, .y = 175, .spd = 1, .direction = 1, .frame = 0, .animCount = 0, .dyingCount = 0, .dying = false, .sprites = {{52 << 1, 56 << 1}, {60 << 1, 64 << 1}, {68 << 1, 72 << 1}, {76 << 1, 80 << 1}}, .deadsprites = {{84 << 1, 88 << 1}, {92 << 1, 96 << 1}}},

						  {.x = 50, .y = 175, .spd = 1, .direction = 2, .frame = 0, .animCount = 0, .dyingCount = 0, .dying = false, .sprites = {{52 << 1, 56 << 1}, {60 << 1, 64 << 1}, {68 << 1, 72 << 1}, {76 << 1, 80 << 1}}, .deadsprites = {{84 << 1, 88 << 1}, {92 << 1, 96 << 1}}},

						  {.x = 150, .y = 20, .spd = 1, .direction = 3, .frame = 0, .animCount = 0, .dyingCount = 0, .dying = false, .sprites = {{52 << 1, 56 << 1}, {60 << 1, 64 << 1}, {68 << 1, 72 << 1}, {76 << 1, 80 << 1}}, .deadsprites = {{84 << 1, 88 << 1}, {92 << 1, 96 << 1}}}};

// Defines a structure for fruit objects
struct Fruit
{
	u16 x;
	u16 y;
	int direction;
	const u16 sprite;
	bool eaten;
};

// Instantiates an array of 4 fruit structs
struct Fruit fruits[4] = {{.x = 200,
						   .y = 200,
						   .direction = 0,
						   .sprite = 128 << 1, 
						   .eaten = false},
						  {.x = 50,
						   .y = 50,
						   .direction = 0,
						   .sprite = 128 << 1, 
						   .eaten = false},
						  {.x = 50,
						   .y = 200,
						   .direction = 0,
						   .sprite = 128 << 1, 
						   .eaten = false},
						  {.x = 200,
						   .y = 50,
						   .direction = 0,
						   .sprite = 128 << 1, 
						   .eaten = false}};

// Defines the structure of a MsPacman object
struct MsPacman
{
	u16 x;
	u16 y;
	const short spd;
	int tolerance;
	int direction;
	int frame;
	int animCount;
	const u16 sprites[4][3];
};

// Instantiates a MsPacman struct
struct MsPacman mspacman = {

	.x = 110,
	.y = 110,
	.spd = 1,
	.tolerance = 10,
	.direction = 0,
	.frame = 0,
	.animCount = 0,
	.sprites = {
		{4 << 1, 8 << 1, 12 << 1},	 // direction 0; right
		{16 << 1, 20 << 1, 24 << 1}, // direction 1; down
		{28 << 1, 32 << 1, 36 << 1}, // direction 2; left
		{40 << 1, 44 << 1, 48 << 1}	 // direction 3; up
	}};

// Increments MsPacman animation frame every 6 in-game frames
void incrementFrame()
{
	if (mspacman.animCount == 6)
	{
		if (mspacman.frame == 2)
		{
			mspacman.frame = 0;
		}
		else
		{
			mspacman.frame++;
		}
		mspacman.animCount = 0;
	}
	else
	{
		mspacman.animCount++;
	}
}

// MsPacman collision detection
bool canMoveUp()
{
	return getForegroundTile(mspacman.x + mspacman.tolerance, mspacman.y) == 0 &&
		   getForegroundTile(mspacman.x + 8, mspacman.y) == 0 &&
		   getForegroundTile(mspacman.x + 15 - mspacman.tolerance, mspacman.y) == 0;
}

bool canMoveDown()
{
	return getForegroundTile(mspacman.x + mspacman.tolerance, mspacman.y + 15) == 0 &&
		   getForegroundTile(mspacman.x + 8, mspacman.y + 15) == 0 &&
		   getForegroundTile(mspacman.x + 15 - mspacman.tolerance, mspacman.y + 15) == 0;
}

bool canMoveLeft()
{
	return getForegroundTile(mspacman.x, mspacman.y + mspacman.tolerance) == 0 &&
		   getForegroundTile(mspacman.x, mspacman.y + 8) == 0 &&
		   getForegroundTile(mspacman.x, mspacman.y + 15 - mspacman.tolerance) == 0;
}

bool canMoveRight()
{
	return getForegroundTile(mspacman.x + 15, mspacman.y + mspacman.tolerance) == 0 &&
		   getForegroundTile(mspacman.x + 15, mspacman.y + 8) == 0 &&
		   getForegroundTile(mspacman.x + 15, mspacman.y + 15 - mspacman.tolerance) == 0;
}

// Ghost collision detection
bool ghostCanMoveUp(struct Ghost *g)
{
	return getForegroundTile(g->x + 2, g->y) == 0 &&
		   getForegroundTile(g->x + 8, g->y) == 0 &&
		   getForegroundTile(g->x + 13, g->y) == 0;
}

bool ghostCanMoveDown(struct Ghost *g)
{
	return getForegroundTile(g->x + 2, g->y + 15) == 0 &&
		   getForegroundTile(g->x + 8, g->y + 15) == 0 &&
		   getForegroundTile(g->x + 13, g->y + 15) == 0;
}

bool ghostCanMoveLeft(struct Ghost *g)
{
	return getForegroundTile(g->x, g->y + 2) == 0 &&
		   getForegroundTile(g->x, g->y + 8) == 0 &&
		   getForegroundTile(g->x, g->y + 13) == 0;
}

bool ghostCanMoveRight(struct Ghost *g)
{
	return getForegroundTile(g->x + 15, g->y + 2) == 0 &&
		   getForegroundTile(g->x + 15, g->y + 8) == 0 &&
		   getForegroundTile(g->x + 15, g->y + 13) == 0;
}

// MsPacman x Ghost collision detection
bool msPacmanGhostCollision(struct Ghost *g)
{
	if (g->dying)
	{
		return false;
	}
	else if (mspacman.x < g->x + 16 &&
			 mspacman.x + 16 > g->x &&
			 mspacman.y < g->y + 16 &&
			 mspacman.y + 16 > g->y)
	{
		ghostsEaten++;
		g->dying = true;
		return true;
	}
	return false;
}

// Function that defines ghost behavior
void ghostBehavior(struct Ghost *g)
{
	// makes sure everything happens every 6 frames instead of every frame
	if (g->animCount == 6)
	{
		// Basic frame counter for animation
		if (g->frame == 0)
		{
			g->frame = 1;
		}
		else
		{
			g->frame = 0;
		}

		// Increases dying count up to 100
		if (g->dying)
		{
			if (g->dyingCount <= 100)

			{
				g->dyingCount++;
			}
		}
		else
		{
			// movement
			switch (g->direction)
			{
			case 0:
				if (ghostCanMoveRight(g))
				{
					g->x += g->spd;
					// Warps from right to left
					if (g->x >= 240)
					{
						g->x = 0;
					}
				}
				// If the ghost can't move, it picks a random direction instead
				else
				{
					g->direction = rand() % 4;
				}
				break;
			case 1:
				if (ghostCanMoveLeft(g))
				{
					g->x -= g->spd;
					// Warps from left to right
					if (g->x <= 0)
					{
						g->x = 240;
					}
				}
				else
				{
					g->direction = rand() % 4;
				}
				break;
			case 2:
				if (ghostCanMoveUp(g))
				{
					g->y -= g->spd;
				}
				else
				{
					g->direction = rand() % 4;
				}
				break;
			case 3:
				if (ghostCanMoveDown(g))
				{
					g->y += g->spd;
				}
				else
				{
					g->direction = rand() % 4;
				}
				break;
			// If for whatever reason direction is not between 1 and 4, set direction to a random number
			default:
				g->direction = rand() % 4;
				break;
			}
		}
	}
	else
	{
		g->animCount++;
	}
}

// MsPacman x Fruit collison
bool msPacmanFruitCollison(struct Fruit *f)
{
	if (f->eaten)
	{
		return false;
	}
	if (mspacman.x < f->x + 16 &&
		mspacman.x + 16 > f->x &&
		mspacman.y < f->y + 16 &&
		mspacman.y + 16 > f->y)
	{
		fruitsEaten++;
		f->eaten = true;
		return true;
	}
	return false;
}

// Fruit collision detection
bool fruitCanMoveUp(struct Fruit *f)
{
	return getForegroundTile(f->x + 2, f->y) == 0 &&
		   getForegroundTile(f->x + 8, f->y) == 0 &&
		   getForegroundTile(f->x + 13, f->y) == 0;
}
bool fruitCanMoveDown(struct Fruit *f)
{
	return getForegroundTile(f->x + 2, f->y + 15) == 0 &&
		   getForegroundTile(f->x + 8, f->y + 15) == 0 &&
		   getForegroundTile(f->x + 13, f->y + 15) == 0;
}
bool fruitCanMoveLeft(struct Fruit *f)
{
	return getForegroundTile(f->x, f->y + 2) == 0 &&
		   getForegroundTile(f->x, f->y + 8) == 0 &&
		   getForegroundTile(f->x, f->y + 13) == 0;
}
bool fruitCanMoveRight(struct Fruit *f)
{
	return getForegroundTile(f->x + 15, f->y + 2) == 0 &&
		   getForegroundTile(f->x + 15, f->y + 8) == 0 &&
		   getForegroundTile(f->x + 15, f->y + 13) == 0;
}

// Function that defines the behavior of active fruits
void fruitBehavior(struct Fruit *f)
{
	switch (f->direction)
	{
	case 0: // right
		if (fruitCanMoveRight(f))
		{
			f->x += 1;
			// Warps from right to left
			if (f->x >= 240)
			{
				f->x = 0;
			}
		}
		else
		{
			// If the fruit can't move in that direction, pick a random direction
			f->direction = rand() % 4;
		}
		break;
	case 1: // left
		if (fruitCanMoveLeft(f))
		{
			f->x -= 1;
			// Warp from left to right
			if (f->x <= 0)
			{
				f->x = 240;
			}
		}
		else
		{
			f->direction = rand() % 4;
		}
		break;
	case 2: // up
		if (fruitCanMoveUp(f))
		{
			f->y -= 1;
		}
		else
		{
			f->direction = rand() % 4;
		}
		break;
	case 3: // down
		if (fruitCanMoveDown(f))
		{
			f->y += 1;
		}
		else
		{
			f->direction = rand() % 4;
		}
		break;
	// If something weird happens, set direction to a random value between 1 and 4
	default:
		f->direction = rand() % 4;
		break;
	}
}

// Changes value of camY if MsPacman goes into a deadzone
void updateCamera()
{
	if (mspacman.y - camY > 120)
	{ // too far down
		camY = mspacman.y - 120;
	}
	else if (mspacman.y - camY < 40)
	{ // too far up
		camY = mspacman.y - 40;
	}

	// Had to look up what registers to change, still not entirely sure hows these work
	REG_BG0VOFS = camY;
	REG_BG1VOFS = camY;
}

// Updates score icons
void updateIcons()
{
	for (int i = 0; i < 4; i++)
	{
		if (i < ghostsEaten)
		{
			SPRT[i + 9].Shape = SQUARE;
		}
		else
		{
			SPRT[i + 9].Shape = 3;
		}
	}
	for (int i = 0; i < 4; i++)
	{
		if (i < fruitsEaten)
		{
			SPRT[i + 13].Shape = SQUARE;
		}
		else
		{
			SPRT[i + 13].Shape = 3;
		}
	}
}
// End user functions

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void)
{
	//---------------------------------------------------------------------------------

	// the vblank interrupt must be enabled for VBlankIntrWait() to work
	// since the default dispatcher handles the bios flags no vblank handler
	// is required; do not remove
	irqInit();
	irqEnable(IRQ_VBLANK);

	///// User add code here /////
	setVideoSettings();
	VBlankIntrWait();
	setBGSettings();
	VBlankIntrWait();
	loadBGPalette(Tiles_pal);
	VBlankIntrWait();
	loadBGTileSet(Tiles_tiles);
	VBlankIntrWait();
	loadGroundTileMap();
	VBlankIntrWait();
	loadForegroundMap();
	VBlankIntrWait();
	loadSpritePalette(MrsPacmanSpriteSheet_pal);
	VBlankIntrWait();
	loadSpriteTileSet(MrsPacmanSpriteSheet_tiles);
	VBlankIntrWait();
	setupMsPacmanSprite();
	VBlankIntrWait();
	for (int i = 1; i <= 4; i++)
	{
		setupGhost(i);
	}
	for (int i = 5; i <= 8; i++)
	{
		setUpFruit(i);
	}
	setupIconSprites();

	while (1)
	{

		// Player control for MsPacman and collision detection for MsPacman
		if (isUp() && canMoveUp())
		{
			mspacman.direction = 3;
			incrementFrame();
			mspacman.y -= mspacman.spd;
		}
		if (isDown() && canMoveDown())
		{
			mspacman.direction = 1;
			incrementFrame();
			mspacman.y += mspacman.spd;
		}
		if (isLeft() && canMoveLeft())
		{
			mspacman.direction = 2;
			incrementFrame();
			mspacman.x -= mspacman.spd;
			// Warps from left to right
			if (mspacman.x <= 0)
			{
				mspacman.x = 240;
			}
		}
		if (isRight() && canMoveRight())
		{
			mspacman.direction = 0;
			incrementFrame();
			mspacman.x += mspacman.spd;
			// Warps from right to left
			if (mspacman.x >= 240)
			{
				mspacman.x = 0;
			}
		}

		// Updates MsPacman sprite and position
		SPRT[0].Character = mspacman.sprites[mspacman.direction][mspacman.frame];
		SPRT[0].X = mspacman.x;
		SPRT[0].Y = mspacman.y - camY;

		for (int i = 0; i < 4; i++)
		{
			// Does ghost behvaior and updates position
			ghostBehavior(&ghosts[i]);
			SPRT[i + 1].X = ghosts[i].x;
			SPRT[i + 1].Y = ghosts[i].y - camY;
			if (ghosts[i].dying && ghosts[i].dyingCount < 60)
			{
				// Sets Ghost to blue and white sprites, depending on the value of dyingCount
				SPRT[i + 1].Character = ghosts[i].deadsprites[ghosts[i].dyingCount / 30][ghosts[i].frame];
			}
			else if (ghosts[i].dying && ghosts[i].dyingCount < 100)
			{
				// Sets Ghost to just eyes
				SPRT[i + 1].Character = (100 + (ghosts[i].direction << 2)) << 1;
			}
			else if (ghosts[i].dying)
			{
				SPRT[i + 1].Shape = 3; // Makes sprite invisible
			}
			else
			{
				// Otherwise, updates Ghost sprite and checks collision
				SPRT[i + 1].Character = ghosts[i].sprites[ghosts[i].direction][ghosts[i].frame];
				msPacmanGhostCollision(&ghosts[i]);
			}
		}

		for (int i = 0; i < 4; i++)
		{
			// Does fruit behavior
			fruitBehavior(&fruits[i]);
			// Sets fruit invisible if collides with mspacman
			if (msPacmanFruitCollison(&fruits[i]))
			{
				SPRT[i + 5].Shape = 3;
			}
			// Updates fruit position
			SPRT[i + 5].X = fruits[i].x;
			SPRT[i + 5].Y = fruits[i].y - camY;
		}

		updateCamera(); // Moves camera
		updateIcons();
		VBlankIntrWait(); // Wait until the next frame starts; Do not remove
	}
}
