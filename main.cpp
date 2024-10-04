#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#define ARENA_IMPLEMENTATION
#include <arena.h> 

#include <raylib.h>
#include <raymath.h>

// :sprite
const Vector4 PLAYER = {0, 0, 16, 16};
const Vector4 TILE = {16, 16, 64, 64};

Arena arena = {};

// :define
#define v2(x, y) Vector2{float(x), float(y)}
#define v2of(val) v2(val, val)
#define v4(x, y, z, w) (Vector4){x, y, z, w}
#define v4xy_v2(v) Vector4{(v).x, (v).y, 0, 0}
#define xy_v4(v) Vector2{v.x, v.y}
#define to_rect(_v4) (Rectangle){_v4.x, _v4.y, _v4.z, _v4.w}

#define ZERO v2of(0)

Vector4 operator*(const Vector4& a, const Vector4& b) {
	return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; 
}

const Vector2 operator*(const Vector2& a, const Vector2& b) {
	return Vector2Multiply(a, b);
}

const Vector2 operator/(const Vector2& a, const Vector2& b) {
	return Vector2Divide(a, b);
}

const Vector2 v2_floor(const Vector2& a) {
	return {floorf(a.x), floorf(a.y)};
}

const Vector2 WINDOW_SIZE = v2(1280, 720);
const Vector2 RENDER_SIZE = v2(640, 360);

typedef enum DrawObjType {
	NONE,
	DRAW_OBJ_QUAD,
	DRAW_OBJ_TEXTURE,
} DrawObjType;

struct DrawObj {
	DrawObjType type;
	Vector4 src;
	Vector4 dest;
	Color tint;
};

typedef struct ListDrawObj {
	DrawObj* items;
	int capacity;
	int count;
} ListDrawObj;

typedef struct Renderer {
	ListDrawObj objs;
	Texture2D atlas;
} Renderer;

Renderer* renderer = NULL;

void renderer_add(DrawObj obj) {
	arena_da_append(&arena, &renderer->objs, obj);
}

void draw_quad(Vector4 dest, Color tint = WHITE) {
	renderer_add({
		.type = DRAW_OBJ_QUAD,
		.dest = dest,
		.tint = tint,
	});
	
}

void draw_texture_v2(Vector4 src_, Vector2 pos, Color tint = WHITE) {
	renderer_add({
		.type = DRAW_OBJ_TEXTURE,
		.src = src_,
		.dest = {pos.x, pos.y, 0, 0},
		.tint = tint,
	});
}

void flush_renderer() {
	for (int i = 0; i < renderer->objs.count; i++) {
		DrawObj it = renderer->objs.items[i];
		switch (it.type) {
			case NONE:
				break;
			case DRAW_OBJ_QUAD:
				DrawRectangleRec(to_rect(it.dest), it.tint);
				break;
			case DRAW_OBJ_TEXTURE:
				DrawTextureRec(renderer->atlas, to_rect(it.src), {it.dest.x, it.dest.y}, it.tint);
				break;
		}	
	}
	renderer->objs.count = 0;
}

// :data
#define MAP_SIZE 100
#define TILE_SIZE 64

typedef struct Player {
	Vector2 pos;
	bool grounded;
} Player;

typedef struct State {
	int map[MAP_SIZE*MAP_SIZE];
	Player player;	
} State;
State *state = NULL;

int main(void) {

	// :raylib
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(WINDOW_SIZE.x, WINDOW_SIZE.y, "ld56");
	SetExitKey(KEY_Q);
	
	// :load
	Texture2D atlas = LoadTexture("./res/atlas.png");

	// :init
	renderer = (Renderer*)arena_alloc(&arena, sizeof(Renderer));
	renderer->objs = {};
	renderer->atlas = atlas;

	Camera2D cam = {};
	cam.zoom = 1.f;
	// fixme: use render_size
	cam.offset = WINDOW_SIZE / v2of(2);

	state = (State*)arena_alloc(&arena, sizeof(State));
	memset(state->map, 0, sizeof(int) * (MAP_SIZE * MAP_SIZE));

	state->player.pos = ZERO;
	state->player.grounded = false;

	// :temp
	for (int y = MAP_SIZE - 10; y < MAP_SIZE; y++) {
		for(int x = 0; x < MAP_SIZE; x++) {
			state->map[y * MAP_SIZE + x] = 1;
		}
	}

	assert(renderer != NULL && "arena returned null");	
	while(!WindowShouldClose()) {

		// :update
		{
			// :player
			{
				Player *player = &state->player;
				if (!player->grounded) {
					player->pos.y += 1;
				}

				// :temp implement consumable input!
				if (IsKeyDown(KEY_A)) {
					player->pos.x -= 1;
				} else if (IsKeyDown(KEY_D)) {
					player->pos.x += 1;
				}

				if (IsKeyPressed(KEY_SPACE)) {
					player->pos.y -= 10;
					player->grounded = false;
				}

				Rectangle player_rect = {player->pos.x, player->pos.y, 16, 16};
				Vector2 world_pos = v2_floor(player->pos / v2of(TILE_SIZE));
				
				int x = (int)world_pos.x;
				int y = (int)world_pos.y;

				if (state->map[y * MAP_SIZE + x] == 1) {
					Rectangle tile_rect = {world_pos.x * TILE_SIZE, world_pos.y * TILE_SIZE, TILE_SIZE, TILE_SIZE};
					if (CheckCollisionRecs(player_rect, tile_rect)) {
						player->pos.y = tile_rect.y - 16;
						player->grounded = true;
					}
				}
			}

			// :cam
			{
				cam.target = state->player.pos;
			}
		}

		for(int y = 0; y < MAP_SIZE; y++) {
			for(int x = 0; x < MAP_SIZE; x++) {
				if (state->map[y * MAP_SIZE + x] == 1) {
					draw_texture_v2(TILE, v2(x, y) * v2of(TILE_SIZE));
				}
			}
		}

		draw_texture_v2(PLAYER, state->player.pos);

		BeginDrawing();
		{
			ClearBackground(BLACK);
			
			BeginMode2D(cam);
			{
				flush_renderer();
			}
			EndMode2D();

			DrawFPS(10, 10);
			DrawText(TextFormat("%f, %f", state->player.pos.x, state->player.pos.y), 10, 30, 20, WHITE);
		}
		EndDrawing();
	}

	CloseWindow();

	return 0;
}
