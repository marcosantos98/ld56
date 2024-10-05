#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#define ARENA_IMPLEMENTATION
#include <arena.h> 

#include <raylib.h>
#include <raymath.h>

#define CUTE_TILED_IMPLEMENTATION
#include <cute_tiled.h>

// :sprite
const Vector4 PLAYER = {1008, 1008, 16, 16};
const Vector4 TILE = {0, 0, 64, 64};

Arena arena = {};
Arena temp_arena = {};

// :define
#define v2(x, y) Vector2{float(x), float(y)}
#define v2of(val) v2(val, val)
#define v4(x, y, z, w) (Vector4){x, y, z, w}
#define v4v2(a, b) Vector4{a.x, a.y, b.x, b.y}
//#define v4xy_v2(v) Vector4{(v).x, (v).y, 0, 0}
#define xy_v4(v) Vector2{v.x, v.y}
#define to_rect(_v4) (Rectangle){_v4.x, _v4.y, _v4.z, _v4.w}
#define to_v2(v) v2(v.x, v.y)
#define to_v4(r) v4(r.x, r.y, r.width, r.height)

#define ZERO v2of(0)

typedef void* rawptr;

int signd(int x) {
    return (x > 0) - (x < 0);
}

float approach(float current, float target, float increase) {
    if (current < target) {
        return fmin(current + increase, target);
    }
    return fmax(current - increase, target);
}

Rectangle rv2(const Vector2& pos, const Vector2& size) {
	return {pos.x, pos.y, size.x, size.y};
}

Vector4 operator*(const Vector4& a, const Vector4& b) {
	return {a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w}; 
}

const Vector2 operator*(const Vector2& a, const Vector2& b) {
	return Vector2Multiply(a, b);
}

const Vector2 operator+(const Vector2& a, const float b) {
	return Vector2Add(a, {b, b});
}

const Vector2 operator+(const Vector2& a, const Vector2& b) {
	return Vector2Add(a, b);
}

const Vector2 operator*(const Vector2& a, const float b) {
	return Vector2Multiply(a, {b, b});
}

const Vector2 operator/(const Vector2& a, const Vector2& b) {
	return Vector2Divide(a, b);
}

const Vector2 operator/(const Vector2& a, const float b) {
	return Vector2Divide(a, {b, b});
}

const Vector2 v2_floor(const Vector2& a) {
	return {floorf(a.x), floorf(a.y)};
}

const Vector2 WINDOW_SIZE = v2(1280, 720);
const Vector2 RENDER_SIZE = v2(640, 360);

// :renderer
typedef enum DrawObjType {
	NONE,
	DRAW_OBJ_QUAD,
	DRAW_OBJ_TEXTURE,
	DRAW_QUAD_LINES,
} DrawObjType;

struct DrawObj {
	DrawObjType type;
	Vector4 src;
	Vector4 dest;
	Color tint;
	float line_tick;
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


void draw_quad_lines(Vector4 dest, float line_tick = 1.f, Color tint = WHITE) {
	renderer_add({
		.type = DRAW_QUAD_LINES,
		.dest = dest,
		.tint = tint,
		.line_tick = line_tick,
	});
}

void draw_texture_v2(Vector4 src, Vector2 pos, Color tint = WHITE) {
	renderer_add({
		.type = DRAW_OBJ_TEXTURE,
		.src = src,
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
			case DRAW_QUAD_LINES:
				DrawRectangleLinesEx(to_rect(it.dest), it.line_tick, it.tint);
				break;
		}	
	}
	renderer->objs.count = 0;
}
// ;renderer

// :entity

enum EntityId {
	EID_NONE,
};

enum EntityType {
	ET_NONE,
	ET_COLLECTABLE,
};

enum EntityProp {
	EP_NONE,
	EP_COLLIDABLE,
};

struct ListEntityProp {
	EntityProp *items;
	int count;
	int capacity;
};

struct Entity {
	Vector2 pos, vel, size, remainder;
	EntityId id;
	EntityType type;
	ListEntityProp props;
	bool valid;
	bool grounded;
	Entity* last_collided;
	rawptr user_data;
};

void en_add_props(Entity* entity, std::initializer_list<EntityProp> props) {
	for(EntityProp prop : props) {
		arena_da_append(&arena, &entity->props, prop);
	}
}

bool en_has_prop(Entity en, EntityProp prop) {
	for (int i = 0; i < en.props.count; i++) {
		if (en.props.items[i] == prop) {
			return true;
		}
	}
	return false;
}

void en_setup(Entity* en, Vector2 pos, Vector2 size) {
	en->pos = pos;
	en->remainder = ZERO;
	en->vel = ZERO;
	en->size = size;
	en->valid = true;
	en->props = {};
}

Rectangle en_box(Entity en) {
	return rv2(en.pos, en.size);
}

// ;entity

// :data
#define MAP_SIZE 100
#define MAX_ENTITIES 1024
#define TILE_SIZE 64


struct iv2 {
	int x, y;
};

#define to_iv2(_v2) iv2{int(_v2.x), int(_v2.y)}

typedef iv2 WorldPos;
WorldPos to_world(Vector2 pos) {
	return to_iv2(v2_floor(pos / TILE_SIZE));
}

Vector2 to_render_pos(WorldPos pos) {
	return to_v2(pos) * TILE_SIZE;
}

struct State {
	int map[MAP_SIZE*MAP_SIZE];
	Entity entities[MAX_ENTITIES];
	Entity* player;	
};
State *state = NULL;

Entity* new_en() {
	for(int i = 0; i < MAX_ENTITIES; i++) {
		if (!state->entities[i].valid) {
			return &state->entities[i];
		}
	}
	assert(true && "Ran out of entities");
	return nullptr;
}

int get_tile(iv2 pos) {
	return state->map[pos.y * MAP_SIZE + pos.x];
}

struct ListEntity {
	Entity* items;
	int count;
	int capacity;
};

ListEntity get_all_with_prop(EntityProp prop, Arena* allocator = &arena) {
	ListEntity list = {};

	for(Entity entity: state->entities) {
		if(!entity.valid) { continue;}
		if(en_has_prop(entity, prop)) {
			arena_da_append(allocator, &list, entity);
		}
	}

	return list;
}

bool en_collides_with(Entity* e, ListEntity entities, Vector2 new_pos) {
	Rectangle rect_at = rv2(new_pos, e->size);
	for (int i = 0; i < entities.count; i++) {
		if(CheckCollisionRecs(rect_at, en_box(entities.items[i]))) {
			return true;
		}
	}
	return false;
}

 void actor_move_x(ListEntity collidables, Entity *e, float amount) {
    e->remainder.x += amount;
    int move = round(e->remainder.x);
    if (move != 0) {
        e->remainder.x -= move;
        int sign = signd(move);
        while (move != 0) {
            if (!en_collides_with(e, collidables, {e->pos.x + sign, e->pos.y})) {
                e->pos.x += sign;
                move -= sign;
            } else {
                if (e->last_collided && !en_has_prop(*e->last_collided, EP_COLLIDABLE)) {
                    e->pos.x += sign;
                    move -= sign;
                } else {
                    break;
                }
            }
        }
    }
}

void actor_move_y(ListEntity collidables, Entity *e, float amount) {
    e->remainder.y += amount;
    int move = round(e->remainder.y);
    if (move != 0) {
        e->remainder.y -= move;
        int sign = signd(move);
        while (move != 0) {
            if (!en_collides_with(e, collidables, {e->pos.x, e->pos.y + sign})) {
                e->pos.y += sign;
                move -= sign;
            } else {
                if (e->last_collided && !en_has_prop(*e->last_collided, EP_COLLIDABLE)) {
                    e->pos.y += sign;
                    move -= sign;
                } else {
                    if (e->vel.y > 0) {
                        e->grounded = true;
                    }
                    e->vel.y = 0;
                    break;
                }
            }
        }
    }
}

// :collectable
Entity* en_collectable(Vector2 pos, EntityId id) {
	Entity* en = new_en();
	en_setup(en, pos, {16, 16});	

	en->type = ET_COLLECTABLE;
	en->id = id;

	en_add_props(en, {EP_COLLIDABLE});

	return en;
}

// :collectable
void en_collectable_update(Entity* self) {
	self->pos.y += sinf(GetTime() * 10) * .5f;	
}

//:collectable
void en_collectable_render(Entity self) {
	draw_quad(v4v2(self.pos, self.size));
}

Entity* en_collider(Vector2 pos, Vector2 size) {
	Entity* en = new_en();
	
	en_setup(en, pos, size);
	en_add_props(en, {EP_COLLIDABLE});

	return en;
}

// :player
Entity* en_player() {
	Entity* en = new_en();
	
	WorldPos pos = {0, MAP_SIZE - 12};
	en_setup(en, to_render_pos(pos), v2of(16));

	return en;
}

// :player
void en_player_update(Entity* self) {
 	
    if (IsKeyDown(KEY_A)) {
        self->vel.x = approach(self->vel.x, -2.0, 22 * GetFrameTime());
    } else if (IsKeyDown(KEY_D)) {
        self->vel.x = approach(self->vel.x, 2.0, 22 * GetFrameTime());
    }

    if (IsKeyPressed(KEY_SPACE) && self->grounded) {
        self->grounded = false;
        self->vel.y = -5;
    }

    if (!IsKeyDown(KEY_A) && !IsKeyDown(KEY_D)) {
        if (self->grounded) {
            self->vel.x = approach(self->vel.x, 0.0, 10 * GetFrameTime());
        } else {
            self->vel.x = approach(self->vel.x, 0.0, 12 * GetFrameTime());
        }
    }

	ListEntity collidables = get_all_with_prop(EP_COLLIDABLE);
	//fixme: add callbacks
    actor_move_x(collidables, self, self->vel.x);
    self->vel.y = approach(self->vel.y, 3.6, 13 * GetFrameTime());
    actor_move_y(collidables, self, self->vel.y);
}

int main(void) {

	// :raylib
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(WINDOW_SIZE.x, WINDOW_SIZE.y, "ld56");
	SetTargetFPS(60);
	SetExitKey(KEY_Q);
	
	// :load
	Texture2D atlas = LoadTexture("./res/atlas.png");
	RenderTexture2D game_texture = LoadRenderTexture(RENDER_SIZE.x, RENDER_SIZE.y);

	// :init
	renderer = (Renderer*)arena_alloc(&arena, sizeof(Renderer));
	renderer->objs = {};
	renderer->atlas = atlas;

	Camera2D cam = {};
	cam.zoom = 1.f;
	cam.offset = RENDER_SIZE / v2of(2);

	state = (State*)arena_alloc(&arena, sizeof(State));
	memset(state->map, 0, sizeof(int) * (MAP_SIZE * MAP_SIZE));
	memset(state->entities, 0, sizeof(Entity) * MAX_ENTITIES);

	state->player = en_player();
	// :temp
	Entity* collectable = en_collectable(to_render_pos({10, MAP_SIZE - 11}), EID_NONE);

	cute_tiled_map_t* map = cute_tiled_load_map_from_file("./res/map.tmj", nullptr);

	cute_tiled_layer_t* layer = map->layers;
	while (layer) {
		

		if (strncmp("Colliders", layer->name.ptr, 9) == 0) {
			cute_tiled_object_t* obj = layer->objects;
			while (obj) {
				
				// fixme: check if point or ellipse or polygon	

				printf("Name: %s\n", obj->name.ptr);
				printf("   x: %f, y: %f, w: %f, h: %f\n", obj->x, obj->y, obj->width, obj->height);

				en_collider(v2(obj->x, obj->y), v2(obj->width, obj->height));

				obj = obj->next;
			}
		} else if (strncmp("map", layer->name.ptr, 3) == 0) {
			assert(layer->data_count == MAP_SIZE * MAP_SIZE && "Map changed size! not handled in code");
			for (int y = MAP_SIZE - 10; y < MAP_SIZE; y++) {
				for(int x = 0; x < MAP_SIZE; x++) {
					state->map[y * MAP_SIZE + x] = layer->data[y * MAP_SIZE + x];
				}
			}
		} else {
			printf("Layer not handled: %s\n", layer->name.ptr);
		}

		layer = layer->next;
	}

	cute_tiled_free_map(map);

	assert(renderer != NULL && "arena returned null");

	// :loop
	while(!WindowShouldClose()) {
	
		arena_reset(&temp_arena);

		// :update
		{
			en_player_update(state->player);

			// :cam
			{
				cam.target = state->player->pos;
			}

			for(int i = 0; i < MAX_ENTITIES; i++) {
				Entity* en	= &state->entities[i];
				if (!en->valid) { continue; };
				switch (en->type) {
					case ET_NONE:
						break;
					case ET_COLLECTABLE:
						en_collectable_update(en);
						break;
				}
			}
		}

		// :game_render
		{
			BeginTextureMode(game_texture);
			{
				ClearBackground(BLANK);
				BeginMode2D(cam);
				{
					// :map
					{
						for(int y = 0; y < MAP_SIZE; y++) {
							for(int x = 0; x < MAP_SIZE; x++) {
								if (get_tile({x, y})) {
									//fixme: this will break!
									Vector4 to_draw = v4(float(get_tile({x, y})) * TILE_SIZE, 0, TILE_SIZE, TILE_SIZE);
									draw_texture_v2(to_draw, v2(x, y) * TILE_SIZE);
								}
							}
						}
					}

					// :player
					{
						draw_texture_v2(PLAYER, state->player->pos);
					}

					// :entities
					{
						for (Entity en: state->entities) {
							if (!en.valid) { continue; }
							switch (en.type) {
								case ET_NONE:
									break;
								case ET_COLLECTABLE:
									en_collectable_render(en);
									break;
							}
						}
					}

#if 1
					ListEntity collidables = get_all_with_prop(EP_COLLIDABLE, &temp_arena);
					for(int i = 0; i < collidables.count; i++) {
						draw_quad_lines(to_v4(en_box(collidables.items[i])));
					}
#endif

					flush_renderer();
				}
				EndMode2D();
			}
			EndTextureMode();
		}

		BeginDrawing();
		{
			ClearBackground(BLACK);
		
			float scale = std::min(float(GetScreenWidth()) / RENDER_SIZE.x, float(GetScreenHeight()) / RENDER_SIZE.y);
			DrawTexturePro(
				game_texture.texture,
				{0, 0, float(game_texture.texture.width), float(-game_texture.texture.height)},
				{
					(float(GetScreenWidth()) - (RENDER_SIZE.x * scale)) * 0.5f,
					(float(GetScreenHeight()) - (RENDER_SIZE.y * scale)) * 0.5f,
					RENDER_SIZE.x * scale,
					RENDER_SIZE.y * scale,
				},
				{},
				0,
				WHITE
			);

			DrawFPS(10, 10);
			DrawText(TextFormat("%f, %f", state->player->pos.x, state->player->pos.y), 10, 30, 20, WHITE);
		}
		EndDrawing();
	}

	CloseWindow();

	return 0;
}
