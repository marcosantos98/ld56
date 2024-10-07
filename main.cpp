#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <sys/stat.h>

#define ARENA_IMPLEMENTATION
#include <arena.h> 

#include <raylib.h>
#include <raymath.h>


// :sprite
const Vector4 PLAYER = {1008, 1008, 16, 16};
const Vector4 BIRD = {112, 144, 16, 16};
const Vector4 FLOWER_0 = {208, 0, 16, 16};
const Vector4 FLOWER_SPOT = {208, 16, 16, 16};
const Vector4 THING = {208, 48, 48, 64};
const Vector4 THING_SPOT = {192, 112, 80, 48};

std::map<int, Vector4> tiles_sprites = {
	{0, {0, 0, 16, 16}},
	{5, {80, 0, 16, 16}},
	{64, {0, 16, 16, 16}},
	{128, {0, 32, 16, 16}},
	{192, {0, 48, 16, 16}},

};

Arena arena = {};
Arena temp_arena = {};

// :define
#define TILE_SIZE 16
#define v2(x, y) Vector2{float(x), float(y)}
#define v2of(val) v2(val, val)
#define v4(x, y, z, w) (Vector4){x, y, z, w}
#define v4v2(a, b) Vector4{a.x, a.y, b.x, b.y}
#define v4zw(z, w) Vector4{0, 0, z, w}
#define xyv4(v) v2(v.x, v.y)
#define to_rect(_v4) (Rectangle){_v4.x, _v4.y, _v4.z, _v4.w}
#define to_v2(v) v2(v.x, v.y)
#define to_v4(r) v4(r.x, r.y, r.width, r.height)

#define ZERO v2of(0)

typedef void* rawptr;

void start_of(Vector4 where, Vector4* it) {
	it->x = where.x;
	it->y = where.y;
}

void end_of(Vector4 where, Vector4* it) {
	it->x = where.x + where.z;
	it->y = where.y;
}

void bottom_of(Vector4 where, Vector4* it) {
	it->y = where.y + where.w - it->w;
}

void center(Vector4 where, Vector4* it, int axis) {
	switch (axis) {
		case 0:
			it->x += (where.z - it->z) * 0.5f;
			break;
		case 1:
			it->y += (where.w - it->w) * .5f;
	}
}

enum Side {
	TOP,
	BOTTOM,
	LEFT,
	RIGHT,
};

void pad(Vector4* it, Side side, float amt) {
	switch(side) {
		case TOP:
			it->y += amt;
			break;
		case BOTTOM:
			it->y -= amt;
			break;
		case LEFT:
			it->x += amt;
			break;
	}
}

void below(Vector4 where, Vector4* it) {
	it->y = where.y + where.w;
}

float scale(float xMax, float y, float yMax) {
	return xMax * (y / yMax);
}

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

const Vector2 operator-(const Vector2& a, const Vector2& b) {
	return Vector2Subtract(a, b);
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
	DRAW_OBJ_TEXTURE_PRO,
	DRAW_QUAD_LINES,
	DRAW_OBJ_TEXT,
} DrawObjType;

struct DrawObj {
	DrawObjType type;
	Vector4 src;
	Vector4 dest;
	Color tint;
	float line_tick;
	Texture2D tex;
	const char* text;
	float text_size;
};

struct ListDrawObj {
	DrawObj* items;
	int capacity;
	int count;
};

struct RenderLayer {
	ListDrawObj objs;
};

struct FIFO {
	int* items;
	int count;
	int capacity;
};

int fifo_pop(FIFO* fifo) {
	int val = fifo->items[fifo->count-1];
	fifo->count -= 1;
	return val;
}

void fifo_push(FIFO* fifo, int val) {
	arena_da_append(&arena, fifo, val);
}

#define MAX_LAYERS 1024
struct Renderer {
	RenderLayer layers[MAX_LAYERS];	
	Texture2D atlas;
	int current_layer;
	FIFO layer_stack;
};

Renderer* renderer = NULL;

void push_layer(int layer) {
	fifo_push(&renderer->layer_stack, renderer->current_layer);
	renderer->current_layer = layer;
}

void pop_layer() {
	renderer->current_layer = fifo_pop(&renderer->layer_stack);
}

void renderer_add(DrawObj obj) {
	RenderLayer *layer = &renderer->layers[renderer->current_layer];
	arena_da_append(&arena, &layer->objs, obj);
}

void draw_text(Vector2 dest, const char* text, float text_size, Color tint = WHITE) {
	renderer_add({
		.type = DRAW_OBJ_TEXT,
		.dest = v4(dest.x, dest.y, 0, 0),
		.tint = tint,
		.text = text,
		.text_size = text_size,
	});
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

void draw_texture_pro(Texture2D texture, Vector4 src, Vector4 dest, Color tint = WHITE) {
	renderer_add({
		.type = DRAW_OBJ_TEXTURE_PRO,
		.src = src,
		.dest = dest,
		.tint = tint,
		.tex = texture,
	});	
}

void flush_renderer() {
	for(int i = 0; i < MAX_LAYERS; i++) {
		RenderLayer* layer = &renderer->layers[i];
		for (int j = 0; j < layer->objs.count; j++) {
			DrawObj it = layer->objs.items[j];
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
				case DRAW_OBJ_TEXTURE_PRO:
					DrawTexturePro(it.tex, to_rect(it.src), to_rect(it.dest), ZERO, 0, it.tint);
					break;
				case DRAW_OBJ_TEXT:
					DrawText(it.text, (int)it.dest.x, (int)it.dest.y, (int)it.text_size, it.tint);
					break;
			}	
		}
		layer->objs.count = 0;
	}
	assert(renderer->layer_stack.count == 0 && "unclosed layers!");
}
// ;renderer

// :entity

enum EntityId {
	EID_NONE,
	EID_BIRD,
};

enum EntityType {
	ET_NONE,
	ET_COLLECTABLE,
	ET_MOVING_PLAT,
	ET_DOOR,
	ET_FLOWER,
	ET_THING,
	ET_WORKER,
	// :type
};

enum EntityProp {
	EP_NONE,
	EP_COLLIDABLE,
	EP_MOVING,
};

struct ListEntityProp {
	EntityProp *items;
	int count;
	int capacity;
};

struct Entity {
	int handle;
	Vector2 pos, vel, size, remainder;
	EntityId id;
	EntityType type;
	ListEntityProp props;
	bool valid;
	bool grounded;
	Entity* last_collided;
	rawptr user_data;
	float facing;
	Entity* riding;
	bool trigger;
	bool was_selected;
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

Vector2 en_center(Entity en) {
	return {en.pos.x + en.size.x / 2, en.pos.y + en.size.y / 2};
}

void en_invalidate(Entity* en) {
	memset(en, 0, sizeof(Entity));	
}

// ;entity

// :data
#define MAP_SIZE 100
#define MAX_ENTITIES 1024
#define MAX_LIGHTS 1
#define PLAYER_LIGHT_RADIUS 20

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
	Vector2 virtual_mouse;
	Camera2D cam;
	bool show_thing_ui;
	float dt;
	float dt_speed;
	bool show_begin_message;
};
State *state = NULL;

struct ListEntity {
	Entity* items;
	int count;
	int capacity;
};
struct FrameData {
	ListEntity flowers;
};
FrameData fdata = {};

Entity* new_en() {
	for(int i = 0; i < MAX_ENTITIES; i++) {
		if (!state->entities[i].valid) {
			state->entities[i].handle = i;
			return &state->entities[i];
		}
	}
	assert(true && "Ran out of entities");
	return nullptr;
}

int get_tile(iv2 pos) {
	return state->map[pos.y * MAP_SIZE + pos.x];
}



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
			e->last_collided = &state->entities[entities.items[i].handle];
			return true;
		}
	}
	return false;
}

typedef bool(*on_collide)(Entity*);

 void actor_move_x(ListEntity collidables, Entity *e, float amount, on_collide callback = nullptr) {
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
				if (callback) {
					callback(e);
				}
                if (e->last_collided && e->last_collided->trigger) {
                    e->pos.x += sign;
                    move -= sign;
                } else {
                    break;
                }
            }
        }
    }
}

void actor_move_y(ListEntity collidables, Entity *e, float amount, on_collide callback = nullptr) {
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
				if (callback) {
					callback(e);
				}
                if (e->last_collided && e->last_collided->trigger) {
                    e->pos.y += sign;
                    move -= sign;
                } else {
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
	en->trigger = true;

	en_add_props(en, {EP_COLLIDABLE});

	return en;
}

// :collectable
void en_collectable_update(Entity* self) {
	self->pos.y += sinf(GetTime() * 10) * .5f;	
}

//:collectable
void en_collectable_render(Entity self) {
	switch (self.id) {
		case EID_BIRD:
			draw_texture_v2(BIRD, self.pos);
			break;
		default:
			draw_quad(v4v2(self.pos, self.size));
			break;
	}
}

// :collider
Entity* en_collider(Vector2 pos, Vector2 size, bool trigger = false) {
	Entity* en = new_en();
	
	en_setup(en, pos, size);
	en_add_props(en, {EP_COLLIDABLE});

	en->trigger = trigger;

	return en;
}

struct PlayerData {
	bool wall_jump;
	bool wall_jumped;
};

// :player
Entity* en_player() {
	Entity* en = new_en();

	en_setup(en, ZERO, v2of(16));

	en->user_data = arena_alloc(&arena, sizeof(PlayerData));

	return en;
}

// :player
bool player_collide_callback(Entity* self) {
	Entity* other = self->last_collided;
	//printf("%f %f %f %f\n%f %f %f %f\n", 
	//	self->pos.x, self->pos.y, self->size.x, self->size.y,
	//	other->pos.x, other->pos.y, other->size.x, other->size.y 
	//);

	if (other->pos.x == self->pos.x + self->size.x
	 || other->pos.x + other->size.x == self->pos.x) {
		if(!self->grounded) {
			((PlayerData*)self->user_data)->wall_jump = true;
		}
	}

	if (!self->riding) {
		if(other->type == ET_MOVING_PLAT && self->pos.y < other->pos.y) {
			self->riding = other;
		}
	}

	if (other->trigger) {
		printf("Found trigger\n");
	}

	return true;
}

// :player
void en_player_update(Entity* self) {
 	if(self->riding) {
		self->vel.x = (self->riding->vel.x * self->riding->facing) * state->dt;
	}	
  
	PlayerData* data = (PlayerData*)self->user_data;
	
	if (IsKeyDown(KEY_A)) {
		self->facing = -1;	
        self->vel.x = approach(self->vel.x, -2.0, 22 * state->dt);
    } else if (IsKeyDown(KEY_D)) {
		self->facing = 1;
        self->vel.x = approach(self->vel.x, 2.0, 22 * state->dt);
    }

    if (IsKeyPressed(KEY_SPACE) && self->grounded) {
        self->grounded = false;
        self->vel.y = -5;
		self->riding = nullptr;
    }

	if (self->grounded) {
		data->wall_jump = false;
		data->wall_jumped = false;
	}


	if (IsKeyPressed(KEY_SPACE) && data->wall_jump && !data->wall_jumped) {
		self->vel.y = -5;
		data->wall_jumped = true;
	}

    if (!IsKeyDown(KEY_A) && !IsKeyDown(KEY_D)) {
        if (self->grounded) {
            self->vel.x = approach(self->vel.x, 0.0, 10 * state->dt);
        } else {
            self->vel.x = approach(self->vel.x, 0.0, 12 * state->dt);
        }
    }

	ListEntity collidables = get_all_with_prop(EP_COLLIDABLE, &temp_arena);
   	actor_move_x(collidables, self, self->vel.x, player_collide_callback);
   	self->vel.y = approach(self->vel.y, 3.6, 13 * state->dt);
   	actor_move_y(collidables, self, self->vel.y, player_collide_callback);

	if (FloatEquals(self->vel.y, 0)) {
		self->grounded = true;
	}
}
enum Layer {
	L_NONE,
	L_BACK,
	L_FLOWER,
	L_WORKER,
	L_DEBUG_COL,
	L_HUD,
};


// :flower
Entity* en_flower(Vector2 pos) {
	Entity* en = new_en();

	en_setup(en, pos, v2of(TILE_SIZE));
	en->type = ET_FLOWER;

	return en;
}

// :flower
void en_flower_update(Entity* self) {
	// noop
}

// :flower
void en_flower_render(Entity self) {
	push_layer(L_FLOWER);
	draw_texture_v2(FLOWER_0, self.pos);
	pop_layer();
	push_layer(L_BACK);
	draw_texture_v2(FLOWER_SPOT, {self.pos.x, self.pos.y + self.size.y / 2.f});
	pop_layer();
}

struct Light {
	Vector2 pos;
	float radius;
	Color color;

	int loc_pos;
	int loc_radius;
	int loc_color;
};

static Light lights[MAX_LIGHTS] = {0};
static int light_idx = 0;

void update_light_data(Shader s, int id) {
	Light l = lights[id];

	float pos[2] = {l.pos.x, l.pos.y};
	SetShaderValue(s, l.loc_pos, pos, SHADER_UNIFORM_VEC2);

	SetShaderValue(s, l.loc_radius, &l.radius, SHADER_UNIFORM_FLOAT);

	float color[4] = {l.color.r / 255.f, l.color.g / 255.f, l.color.b / 255.f, l.color.a / 255.f};
	SetShaderValue(s, l.loc_color, color, SHADER_UNIFORM_VEC4);
}

void update_all_light_data(Shader s) {
	for (int i = 0; i < MAX_LIGHTS; i++) {
		update_light_data(s, i);	
	}
}

int add_light(Shader s, Vector2 position, float radius, Color color) {
	int id = light_idx;

	lights[id].pos = position;
	lights[id].radius = radius;
	lights[id].color = color;

	lights[id].loc_pos = GetShaderLocation(s, TextFormat("lights[%i].pos", id));
	lights[id].loc_radius = GetShaderLocation(s, TextFormat("lights[%i].radius", id));
	lights[id].loc_color = GetShaderLocation(s, TextFormat("lights[%i].color", id));

	update_light_data(s, id);

	light_idx += 1;

	return id;
}

enum Task {
	TASK_NONE,
	TASK_COLLECT,
	TASK_POLLINATE,
};

struct WorkerData {
	Task task;
	Entity* flower;
};

// :workers
Entity* en_worker(Vector2 pos, Task task) {
	Entity* en = new_en();

	en_setup(en, pos, v2of(10));

	en->type = ET_WORKER;

	WorkerData* data = (WorkerData*)arena_alloc(&arena, sizeof(WorkerData));
	data->task = task;

	en->user_data = data;

	return en;
}

// :worker
void en_worker_update(Entity* self) {

	WorkerData* data = (WorkerData*)self->user_data;

	if (data->flower == nullptr && fdata.flowers.count > 0) {
		int rnd_idx = GetRandomValue(0, fdata.flowers.count - 1);
		data->flower = &state->entities[fdata.flowers.items[rnd_idx].handle];
		assert(!data->flower->was_selected && "Flower is selected!");
		data->flower->was_selected = true;
	}

	if (data->flower != nullptr) {

		self->pos = Vector2MoveTowards(self->pos, data->flower->pos, 100 * state->dt);

		if (Vector2Equals(self->pos, data->flower->pos)) {
			en_invalidate(data->flower);
			en_invalidate(self);
		}
	}
}

// :worker
void en_worker_render(Entity self) {
	push_layer(L_WORKER);
	draw_quad(to_v4(en_box(self)), GOLD);
	pop_layer();
}

// :thing
struct ThingData {
	ListEntity workers;
	Task current_task;
	float perform_task_time;
	int food_amt;
	int worker_amt;
};

#define PERFORM_TASK_TIME 1.2f
#define WORKER_AMT 20
#define START_FOOD_AMT 100

// :thing
Entity* en_thing(Vector2 pos, Vector2 size) {
	Entity* en = new_en();

	en_setup(en, pos, size);
	en->type = ET_THING;

	ThingData* data = (ThingData*)arena_alloc(&arena, sizeof(ThingData));
	memset(data, 0, sizeof(ThingData));
	data->perform_task_time = PERFORM_TASK_TIME;
	data->current_task = TASK_NONE;
	data->worker_amt = WORKER_AMT;
	data->food_amt = START_FOOD_AMT;

	en->user_data = data;

	return en;
}

// :thing
void en_thing_update(Entity* self) {

	if(IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
		if (CheckCollisionPointRec(GetScreenToWorld2D(state->virtual_mouse, state->cam), en_box(*self))) {
			state->show_thing_ui = true;
		}
	}

	ThingData* data = (ThingData*)self->user_data;
	
	switch (data->current_task) {
		case TASK_COLLECT:
			data->perform_task_time -= state->dt * state->dt_speed;
			if (data->perform_task_time < 0 && fdata.flowers.count > 0) {
				en_worker(en_center(*self), data->current_task);
				data->perform_task_time = PERFORM_TASK_TIME;
			}
			break;
	}

	
}

// :thing
void en_thing_render(Entity self) {
	push_layer(L_DEBUG_COL);
	draw_texture_v2(THING, self.pos);
	pop_layer();
	draw_texture_v2(THING_SPOT, {(self.pos.x + (self.size.x - THING_SPOT.z) * .5f), (self.pos.y + self.size.y / 2)});
}



bool ui_btn(Vector2 pos, const char* text, float text_size) {

	Vector4 dest = v4zw(96, 32);
	dest.x = pos.x;
	dest.y = pos.y;

	bool hover = false;
	bool clicked = false;

	if (CheckCollisionPointRec(state->virtual_mouse, to_rect(dest))) {
		hover = true;
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
			clicked = true;
		}
	}

	float text_sz = MeasureText(text, text_size);
	Vector2 text_pos = { 
		(dest.z - text_sz) * .5f, 
		(dest.w - text_size) * .5f,
	};

	text_pos = xyv4(dest) + text_pos;

	draw_texture_v2({128, hover ? 240.f : 208.f, 96, 32}, pos);
	draw_text(text_pos, text, text_size);

	return clicked;
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
	RenderTexture2D light_texture = LoadRenderTexture(RENDER_SIZE.x, RENDER_SIZE.y);
	Shader light_shader = LoadShader(nullptr, "./res/light_frag.glsl");

	// :init
	renderer = (Renderer*)arena_alloc(&arena, sizeof(Renderer));
	memset(renderer->layers, 0, sizeof(RenderLayer) * MAX_LAYERS);
	renderer->layer_stack = {0};
	renderer->current_layer = 0;
	renderer->atlas = atlas;
	
	state = (State*)arena_alloc(&arena, sizeof(State));
	memset(state->map, -1, sizeof(int) * (MAP_SIZE * MAP_SIZE));
	memset(state->entities, 0, sizeof(Entity) * MAX_ENTITIES);
	state->dt_speed = 1;	
	state->cam = Camera2D{};
	state->cam.zoom = 1.f;
	state->cam.offset = RENDER_SIZE / v2of(2);

	state->show_begin_message = true;

	Vector2 player_size = v2(48, 64);
	Vector2 player_pos = ZERO - (player_size / 2);
	state->player = en_thing(player_pos, player_size);

	bool lights_on = false;
	add_light(light_shader, ZERO, PLAYER_LIGHT_RADIUS * TILE_SIZE, {255, 200, 37, 255});

	assert(renderer != NULL && "arena returned null");


	float flower_spawn_time = 2.f;
	
	for (int i = 0; i < 256; i++) {
		Vector2 pos = v2(
			GetRandomValue(-RENDER_SIZE.x/2, RENDER_SIZE.x/2),
			GetRandomValue(-RENDER_SIZE.y/2, RENDER_SIZE.y/2)
		);
			
		bool in_player = CheckCollisionPointRec(pos, to_rect(v4v2(player_pos, v2(48, 64))));
		bool out_of_bounds = pos.x + 16 > RENDER_SIZE.x / 2 || pos.x < -RENDER_SIZE.x / 2 || pos.y + 16 > RENDER_SIZE.x / 2 || pos.y < -RENDER_SIZE.x / 2;
		if(!in_player && !out_of_bounds) {
			en_flower(pos);
		}
	}

	// :loop
	while(!WindowShouldClose()) {

		state->dt = GetFrameTime();
		state->dt *= state->dt_speed;

		arena_reset(&temp_arena);
		fdata.flowers = {0};

		float scale = fmin(WINDOW_SIZE.x / RENDER_SIZE.x, WINDOW_SIZE.y / RENDER_SIZE.y);
		state->virtual_mouse = (GetMousePosition() - (WINDOW_SIZE - (RENDER_SIZE * scale)) * .5) / scale;
		state->virtual_mouse = Vector2Clamp(state->virtual_mouse, ZERO, RENDER_SIZE);

		// :update
		{
			if (state->show_begin_message && IsKeyPressed(KEY_ENTER)) {
				state->show_begin_message = false;
			}

			// :spawn
			//{
			//	flower_spawn_time -= state->dt * state->dt_speed;
			//	if (flower_spawn_time < 0) {
			//		Vector2 pos = v2(
			//				GetRandomValue(-RENDER_SIZE.x/2, RENDER_SIZE.x/2),
			//				GetRandomValue(-RENDER_SIZE.y/2, RENDER_SIZE.y/2)
			//		);
		
			//		bool in_player = CheckCollisionPointRec(pos, to_rect(v4v2(player_pos, v2(48, 64))));
			//		bool out_of_bounds = pos.x + 16 > RENDER_SIZE.x / 2 || pos.x < -RENDER_SIZE.x / 2 || pos.y + 16 > RENDER_SIZE.x / 2 || pos.y < -RENDER_SIZE.x / 2;
			//		if(!in_player && !out_of_bounds) {
			//			en_flower(pos);
			//		}


			//		flower_spawn_time = 2.f;
			//	}
			//}

			// :gather unselected flowers
			{
				for(Entity en : state->entities) {
					if(en.valid && en.type == ET_FLOWER && !en.was_selected) {
						arena_da_append(&temp_arena, &fdata.flowers, en);
					}
				}
			}

			// :debug
			{
				if (IsKeyPressed(KEY_L)) {
					lights_on = !lights_on;
				} else if(IsKeyPressed(KEY_K)) {
					state->dt_speed += 1;
				} else if(IsKeyPressed(KEY_J)) {
					state->dt_speed -= 1;
				}
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
					case ET_THING:
						en_thing_update(en);
						break;
					case ET_WORKER:
						en_worker_update(en);
						break;
				}
			}
		}

		

		// :game_render
		{
			BeginTextureMode(game_texture);
			{
				ClearBackground(BLACK);

				BeginMode2D(state->cam);
				{
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
								case ET_FLOWER:
									en_flower_render(en);
									break;
								case ET_THING:
									en_thing_render(en);
									break;
								case ET_WORKER:
									en_worker_render(en);
									break;
							}
						}
					}

#if 0
					push_layer(L_DEBUG_COL);
					ListEntity collidables = get_all_with_prop(EP_COLLIDABLE, &temp_arena);
					for(int i = 0; i < collidables.count; i++) {
						draw_quad_lines(to_v4(en_box(collidables.items[i])));
					}
					pop_layer();
#endif

					flush_renderer();
				}
				EndMode2D();

				// :ui
				{
					if (state->show_thing_ui) {
						Vector4 sprite = v4(528, 0, 304, 224);
						Vector4 dest = v4zw(sprite.z, sprite.w);
						dest.x = (RENDER_SIZE.x - dest.z) * .5f;
						dest.y = (RENDER_SIZE.y - dest.w) * .5f;

						float size = MeasureText("Perform task:", 20);
						Vector4 title_dest = v4zw(size, 20);
						start_of(dest, &title_dest);
						center(dest, &title_dest, 0);
						pad(&title_dest, TOP, 10);
						
						draw_texture_v2(sprite, xyv4(dest));
						draw_text(xyv4(title_dest), "Perform task:", 20);
					}
				}

				// :hud
				{
					push_layer(L_HUD);
					{
						Vector4 food_icon = {144, 160, 32, 32};
						Vector4 food_dest = v4(10, 10, 32, 32);

						ThingData data = *(ThingData*)state->player->user_data;
						const char* foodstr = TextFormat("%d", data.food_amt);
						float text_size = MeasureText(foodstr, 20);
						Vector4 food_amt = v4zw(text_size, 20);
						end_of(food_dest, &food_amt);
						center(food_dest, &food_amt, 1);

						Vector4 workers_dest = v4zw(32, 32);
						start_of(food_dest, &workers_dest);
						below(food_dest, &workers_dest);
						pad(&food_amt, LEFT, 10);
						
						const char* workerstr = TextFormat("%d", data.worker_amt);
						float workker_sz = MeasureText(workerstr, 20);
						Vector4 worker_amt = v4zw(workker_sz, 20);
						end_of(workers_dest, &worker_amt);
						center(workers_dest, &worker_amt, 1);
						pad(&worker_amt, LEFT, 10);

						draw_texture_v2(food_icon, xyv4(food_dest));
						draw_text(xyv4(food_amt), foodstr, 20);
						draw_quad(workers_dest, GOLD);
						draw_text(xyv4(worker_amt), workerstr, 20);
					}
					pop_layer();
				}

				// :message
				{
					if (state->show_begin_message) {
						Vector4 sprite = v4(288, 0, 224, 304);
						Vector4 dest = v4zw(sprite.z, sprite.w);
						dest.x = (RENDER_SIZE.x - dest.z) * .5f;
						dest.y = (RENDER_SIZE.y - dest.w) * .5f;

						float size = MeasureText("Welcome", 20);
						Vector4 title_dest = v4zw(size, 20);
						start_of(dest, &title_dest);
						center(dest, &title_dest, 0);
						pad(&title_dest, TOP, 10);

						Vector4 ok_btn = v4zw(100, 25);
						start_of(dest, &ok_btn);
						bottom_of(dest, &ok_btn);
						center(dest, &ok_btn, 0);
						pad(&ok_btn, BOTTOM, 10);

						constexpr int message_len = 15;
						const char* messages[message_len] = {
							"It seems like you have been",
							"given the task of managing this colony.",
							"Try keeping it alive by managing ants.",
							"They can collect food, build defenses,",
							"pollinate and reproduce.",
							"",
							"Be aware the colony can't run out of",
							"food, or the ant's will leave.",
							"Every time your ants perform a task,",
							"you will be asked to give",
							"another task to them.",
							"",
							"Ocassionaly predators may appear, so try",
							"to have that in mind when making your",
							"ants go outside.",
						};
						
						draw_texture_v2(sprite, xyv4(dest));
						draw_text(xyv4(title_dest), "Welcome", 20);

						float new_y = 0.f;
						for (int i = 0; i < message_len; i++) {
							float message_sz = MeasureText(messages[i], 10);
							Vector4 message_dest = v4zw(message_sz, 10);
							start_of(dest, &message_dest);
							below(title_dest, &message_dest);
							pad(&message_dest, TOP, 10);
							center(dest, &message_dest, 0);
							message_dest.y += i * message_dest.w;
							draw_text(xyv4(message_dest), messages[i], 10);
						}

						if (ui_btn(xyv4(ok_btn), "Start", 10)) {
							state->show_begin_message = false;
							state->show_thing_ui = true;
						}
					}
				}

				flush_renderer();
			}
			EndTextureMode();
		}
		
		// :light_texture
		{
			BeginTextureMode(light_texture);
			{
				ClearBackground(BLACK);
				
				BeginShaderMode(light_shader);
				{
					update_all_light_data(light_shader);	

					DrawTexturePro(
							game_texture.texture, 
							{0, 0, float(game_texture.texture.width), float(game_texture.texture.height)},
							{0, 0, RENDER_SIZE.x, RENDER_SIZE.y},
							ZERO,
							0, 
							WHITE
					);
				}
				EndShaderMode();
			}
			EndTextureMode();
		}
		
		RenderTexture2D final = game_texture;
		if(lights_on) {
			final = light_texture;
		}

		BeginDrawing();
		{
			ClearBackground(BLACK);
		
			float scale = std::min(float(GetScreenWidth()) / RENDER_SIZE.x, float(GetScreenHeight()) / RENDER_SIZE.y);
			DrawTexturePro(
				final.texture,
				{0, 0, float(final.texture.width), float(-final.texture.height)},
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

		
			DrawFPS(10, WINDOW_SIZE.y - 20);
		}
		EndDrawing();
	}

	CloseWindow();

	return 0;
}
