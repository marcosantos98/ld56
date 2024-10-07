#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <sys/stat.h>

#define ARENA_IMPLEMENTATION
#include <arena.h> 

#define MAX_TEXTFORMAT_BUFFERS 4
#define MAX_TEXT_BUFFER_LENGTH 4096
#include <raylib.h>
#include <raymath.h>
#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

// :sprite
const Vector4 PLAYER = {1008, 1008, 16, 16};
const Vector4 BIRD = {112, 144, 16, 16};
const Vector4 FLOWER_0 = {208, 0, 16, 16};
const Vector4 FLOWER_SPOT = {208, 16, 16, 16};
const Vector4 THING = {208, 48, 48, 64};
const Vector4 THING_SPOT = {192, 112, 80, 48};
const Vector4 FOOD_ICON = {144, 160, 32, 32};
const Vector4 WORKER_ICON = {176, 160, 32, 32};
const Vector4 DEFENSE_BUILDING = {224, 320, 32, 48};
const Vector4 PREDATOR = {224, 160, 32, 32};
const Vector4 FIREBALL = {208, 160, 16, 16};

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

struct Time {
	int h, m, s;
};

Time seconds_to_hm(int seconds) {
	Time t = {};
	t.h = int(seconds / 3600);
	t.m = int(seconds - (3600 * t.h)) / 60;
	t.s = int(seconds - (3600 * t.h) - (t.m * 60));
	return t;
}

Vector4 grow(Vector4 old, float amt) {
	return {
		old.x - amt, 
		old.y - amt,
		old.z + amt * 2, 
		old.w + amt * 2,
	};
}

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
		case RIGHT:
			it->x -= amt;
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
	arena_da_append(&temp_arena, fifo, val);
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
	ET_DEFENSE,
	ET_FLOWER,
	ET_THING,
	ET_WORKER,
	ET_PREDATOR,
	ET_FIREBALL,
	// :type
};

enum EntityProp {
	EP_NONE,
	EP_ATTACKABLE,
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
	int health;
	bool attacked;
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
#define MAX_ENTITIES 2046 
#define MAX_LIGHTS 1
#define PLAYER_LIGHT_RADIUS 20
#define TIME_FOR_PREDATOR 180

enum Task {
	TASK_NONE,
	TASK_COLLECT,
	TASK_REPRODUCE,
	TASK_DEFENSE,
};

// :thing
struct ThingData {
	Task current_task;
	float perform_task_time;
	int food_amt;
	int worker_amt;
	int last_worker_amt;
};

struct State {
	Entity entities[MAX_ENTITIES];
	Entity* player;
	Entity *predator;
	ThingData* thing_data;
	Vector2 virtual_mouse;
	Camera2D cam;
	bool show_thing_ui;
	float dt;
	float dt_speed;
	bool show_begin_message;
	float time_for_predator;
	int flower_cnt;
	Sound remove_flower;
	Sound shoot;
	Sound died;
	bool lost;
	bool win;
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

ListEntity get_all_with_type(EntityType type) {
	ListEntity list = {};

	for(Entity entity: state->entities) {
		if(!entity.valid) { continue;}
		if( entity.type == type) {
			arena_da_append(&temp_arena, &list, entity);
		}
	}

	return list;
}

enum Layer {
	L_NONE,
	L_BACK,
	L_FLOWER,
	L_WORKER,
	L_DEBUG_COL,
	L_HUD,
};


// :fireball
Entity* en_fireball(Vector2 pos) {
	Entity* en = new_en();

	en_setup(en, pos, v2of(16));

	en->type = ET_FIREBALL;

	return en;
}

//:fireball
void en_fireball_update(Entity* self) {
	if (state->predator == nullptr) return;
	if (!CheckCollisionRecs(en_box(*self), en_box(*state->predator))) {
		self->pos = Vector2MoveTowards(self->pos, state->predator->pos, 200 * state->dt);
	} else {
		state->predator->health -= 2;
		en_invalidate(self);
	}
}

void en_fireball_render(Entity self) {
	push_layer(L_HUD);
	draw_texture_v2(FIREBALL, self.pos);	
	pop_layer();
}

// :defense
struct DefenseData {
	float shoot_time;
};

// :defense
Entity* en_defense(Vector2 pos, Vector2 size) {
	Entity* en = new_en();
	en_setup(en, pos, size);	

	en->type = ET_DEFENSE;

	DefenseData *data = (DefenseData*)arena_alloc(&arena, sizeof(DefenseData));
	data->shoot_time = .12f;

	en->health = 3;

	en->user_data = data;

	en_add_props(en, {EP_ATTACKABLE});
	return en;
}

void en_defense_update(Entity* self) {
	if (state->predator == nullptr) return;

	DefenseData* data = (DefenseData*)self->user_data;
	data->shoot_time -= state->dt;
	if (Vector2Distance(self->pos, state->predator->pos) < RENDER_SIZE.x / 2 && data->shoot_time < 0) {
		en_fireball(self->pos);
		PlaySound(state->shoot);
		data->shoot_time = 0.12;
	}

	if(self->health <= 0) {
		PlaySound(state->died);
		en_invalidate(self);
	}
}

void en_defense_render(Entity self) {
	push_layer(L_DEBUG_COL);
	draw_texture_v2(DEFENSE_BUILDING, self.pos);
	pop_layer();
}

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

struct PredatorData {
	int handle;
	float attack_time;
};

#define PREDATOR_HP 300 

// :predator
Entity* en_predator(Vector2 pos, Vector2 size) {
	Entity* en = new_en();

	en_setup(en, pos, size);
	en->type = ET_PREDATOR;

	en->health = PREDATOR_HP;

	PredatorData *data = (PredatorData*)arena_alloc(&arena, sizeof(PredatorData));
	data->handle = -1;
	data->attack_time = 1.f;
	en->user_data = data;
	return en;
}

// :predator
void en_predator_update(Entity* self) {
	
	PredatorData *data = (PredatorData*)self->user_data;
	
	if (data->handle == -1) {
		while (data->handle == -1) {
			ListEntity defense = get_all_with_prop(EP_ATTACKABLE, &temp_arena);
			if(defense.count == 1) {
				data->handle = defense.items[0].handle;
				break;
			} else {
				int rnd_idx = GetRandomValue(0, defense.count - 1);
				if (defense.items[rnd_idx].type == ET_THING) {continue;}
				data->handle = defense.items[rnd_idx].handle;
				break;
			}
		}
	}

	if (!state->entities[data->handle].valid) {
		data->handle = -1;
		return;
	}

	self->pos = Vector2MoveTowards(self->pos, state->entities[data->handle].pos, 60 * state->dt);

	data->attack_time -= state->dt;
	if (CheckCollisionRecs(en_box(*self), en_box(state->entities[data->handle])) && data->attack_time < 0) {
		Entity *en = &state->entities[data->handle];
		en->health -= 1;
		data->attack_time = 1;
		en->attacked = true;
	}

	if (self->health <= 0) {
		state->win = true;
		en_invalidate(self);
	}
}

// :predator
void en_predator_render(Entity self) {
	push_layer(L_DEBUG_COL);
	draw_texture_v2(PREDATOR, self.pos);
	pop_layer();
}

struct WorkerData {
	Task task;
	int handle;
};

// :workers
Entity* en_worker(Vector2 pos, Task task) {
	Entity* en = new_en();

	en_setup(en, pos, v2of(10));

	en->type = ET_WORKER;

	WorkerData* data = (WorkerData*)arena_alloc(&arena, sizeof(WorkerData));
	data->task = task;
	data->handle = -1;
	en->user_data = data;

	return en;
}


// :worker
void en_worker_update(Entity* self) {

	WorkerData* data = (WorkerData*)self->user_data;
	ThingData* thing_data = (ThingData*)state->player->user_data;

	if (data->handle == -1 && fdata.flowers.count > 0) {
		int rnd_idx = GetRandomValue(0, fdata.flowers.count - 1);
		data->handle = fdata.flowers.items[rnd_idx].handle;
		state->entities[data->handle].was_selected = true;
	}

	if (data->handle != -1 && state->entities[data->handle].valid) {

		self->pos = Vector2MoveTowards(self->pos, state->entities[data->handle].pos, 100 * state->dt);

		if (Vector2Equals(self->pos, state->entities[data->handle].pos)) {
			en_invalidate(&state->entities[data->handle]);
			en_invalidate(self);
			state->flower_cnt -= 1;
			PlaySound(state->remove_flower);
			thing_data->food_amt += GetRandomValue(2, 5);	
		}
	}
}

// :worker
void en_worker_render(Entity self) {
	push_layer(L_WORKER);
	draw_texture_v2(WORKER_ICON, self.pos);
	pop_layer();
}

#define PERFORM_TASK_TIME .8f
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
	en->health = 100;

	en_add_props(en, {EP_ATTACKABLE});

	return en;
}

// :thing
void en_thing_update(Entity* self) {

	ThingData* data = (ThingData*)self->user_data;
	
	switch (data->current_task) {
		case TASK_NONE:
			break;
		case TASK_COLLECT:
			if(data->worker_amt > 0) {
				data->perform_task_time -= state->dt * state->dt_speed;
				if (data->perform_task_time < 0 && fdata.flowers.count > 0) {
					en_worker(en_center(*self), data->current_task);
					data->perform_task_time = PERFORM_TASK_TIME;
					data->worker_amt -= 1;
					data->food_amt -= 1;
				}
			} else {
				data->current_task = TASK_NONE;
				data->worker_amt = data->last_worker_amt;
			}
			break;
		case TASK_DEFENSE:
		{
			data->food_amt -= 200;
			data->worker_amt -= 10;
			Vector2 pos = v2(
					GetRandomValue(-RENDER_SIZE.x/2, RENDER_SIZE.x/2),
					GetRandomValue(-RENDER_SIZE.y/2, RENDER_SIZE.y/2)
			);
		
			bool in_player = CheckCollisionPointRec(pos, to_rect(v4v2(self->pos, v2(48, 64))));
			
			en_defense(pos, v2(DEFENSE_BUILDING.z, DEFENSE_BUILDING.w));

			data->current_task = TASK_NONE;
		}
		break;
		case TASK_REPRODUCE:
		{
			data->food_amt -= 2 * (data->worker_amt / 2);
			data->worker_amt += data->worker_amt / 2;
			data->current_task = TASK_NONE;
		}
		break;
	}

	if (self->health <= 0) {
		state->lost = true;
	}

	if (self->attacked) {
		ListEntity def = get_all_with_prop(EP_ATTACKABLE);
		if (def.count == 1) {
			state->lost = true;
		}
	}
}

// :thing
void en_thing_render(Entity self) {
	push_layer(L_DEBUG_COL);
	draw_texture_v2(THING, self.pos);
	pop_layer();
	draw_texture_v2(THING_SPOT, {(self.pos.x + (self.size.x - THING_SPOT.z) * .5f), (self.pos.y + self.size.y / 2)});
}



bool ui_btn(Vector2 pos, const char* text, float text_size, bool can_click = true) {

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

	if(!can_click) {
		draw_quad(v4(pos.x, pos.y, 96, 32), ColorAlpha(GRAY, .8));
	}

	return clicked && can_click;
}

Music loop_1;
Music predator_music;
Sound ui_click;
Sound hover_sound;
Music music;
float volume = 0;
float flower_spawn_time = 0.f;
bool in_predator = false;
Vector2 player_pos;
RenderTexture2D game_texture;
RenderTexture2D light_texture;
RenderTexture2D ui_texture;

void update_frame() {
	UpdateMusicStream(music);
		
		if (volume < .7) {
			volume = fminf(volume + 0.2 * GetFrameTime(), .7);
			SetMusicVolume(music, volume);
		}

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
				state->show_thing_ui = true;
			}

			if (state->thing_data->current_task == TASK_NONE && !state->show_begin_message && !in_predator) {
				state->show_thing_ui = true;
				state->dt_speed = 1;
			}

			if (!state->show_thing_ui && !state->show_begin_message) {
				state->time_for_predator -= state->dt * state->dt_speed;
			}

			if (state->time_for_predator <= 0) {
				if (!in_predator) {
					state->dt_speed = 1;
					state->predator = en_predator(v2(0, -RENDER_SIZE.y / 2), v2(PREDATOR.z, PREDATOR.w));
					StopMusicStream(music);
					music = predator_music;
					volume = 0.f;
					PlayMusicStream(music);
					in_predator =  true;
				}
			}

			// :spawn
			{
				flower_spawn_time -= state->dt * state->dt_speed;
				if (flower_spawn_time < 0 && state->flower_cnt < 300) {
					Vector2 pos = v2(
							GetRandomValue(-RENDER_SIZE.x/2, RENDER_SIZE.x/2),
							GetRandomValue(-RENDER_SIZE.y/2, RENDER_SIZE.y/2)
					);
		
					bool in_player = CheckCollisionPointRec(pos, to_rect(v4v2(player_pos, v2(48, 64))));
					bool out_of_bounds = pos.x + 16 > RENDER_SIZE.x / 2 || pos.x < -RENDER_SIZE.x / 2 || pos.y + 16 > RENDER_SIZE.x / 2 || pos.y < -RENDER_SIZE.x / 2;
					if(!in_player && !out_of_bounds) {
						en_flower(pos);
						state->flower_cnt += 1;
					}


					flower_spawn_time = 2.f;
				}
			}

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
				if(IsKeyPressed(KEY_K)) {
					state->dt_speed += 1;
				} else if(IsKeyPressed(KEY_J)) {
					state->dt_speed -= 1;
				}

				state->dt_speed = Clamp(state->dt_speed, 1, 10);
			}

			for(int i = 0; i < MAX_ENTITIES; i++) {
				Entity* en	= &state->entities[i];
				if (!en->valid) { continue; };
				switch (en->type) {
					case ET_NONE:
					case ET_FLOWER:
						break;
					case ET_DEFENSE:
						en_defense_update(en);
						break;
					case ET_THING:
						en_thing_update(en);
						break;
					case ET_WORKER:
						en_worker_update(en);
						break;
					case ET_PREDATOR:
						en_predator_update(en);
						break;
					case ET_FIREBALL:
						en_fireball_update(en);
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
								case ET_DEFENSE:
									en_defense_render(en);
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
								case ET_PREDATOR:
									en_predator_render(en);
									break;
								case ET_FIREBALL:
									en_fireball_render(en);
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

							}
			EndTextureMode();
		}

		
		
		// :light_texture
		{
			BeginTextureMode(light_texture);
			{
				ClearBackground(BLACK);
				

					DrawTexturePro(
							game_texture.texture, 
							{0, 0, float(game_texture.texture.width)
							, float(game_texture.texture.height)},
							{0, 0, RENDER_SIZE.x, RENDER_SIZE.y},
							ZERO,
							0, 
							WHITE
					);
			}
			EndTextureMode();
		}
		
		
		
		BeginTextureMode(ui_texture);
		{
			ClearBackground(BLANK);
				DrawTexturePro(
							light_texture.texture, 
							{0, 0, float(light_texture.texture.width),
							float(light_texture.texture.height)},
							{0, 0, RENDER_SIZE.x, RENDER_SIZE.y},
							ZERO,
							0, 
							WHITE
				);

				// :ui
				{
					if (state->show_thing_ui) {

						Vector4 tasks = v4(0, 416, 446, 224);
							
						Vector4 sprite = v4(0, 416, 576, 224);
						Vector4 dest = v4zw(sprite.z, sprite.w);
						dest.x = (RENDER_SIZE.x - dest.z) * .5f;
						dest.y = (RENDER_SIZE.y - dest.w) * .5f;

						float size = MeasureText("Perform task:", 20);
						Vector4 title_dest = v4zw(size, 20);
						start_of(dest, &title_dest);
						pad(&title_dest, TOP, 10);
						pad(&title_dest, LEFT, 10);
						
						draw_texture_v2(sprite, xyv4(dest));
						draw_text(xyv4(title_dest), "Perform task:", 20);


						const char* task_name[3] = {
							"Collect", "Build Defense", "Reproduce",
						};

						static int selected = -1;
						static int hover = 0;
						for(int i = 0; i < 3; i++) {
							Vector4 collect = v4zw(132, 132);
							start_of(dest, &collect);
							center(dest, &collect, 1);
							pad(&collect, LEFT, 10);
							collect.x += i * (collect.z + 10);

							if (CheckCollisionPointRec(state->virtual_mouse, to_rect(collect))) {
								collect = grow(collect, 5);
								if (hover != i)
									PlaySound(hover_sound);
								hover = i;
								if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
									selected = i;
									PlaySound(ui_click);
								}
							}
							
							Vector4 text_dest = v4zw(float(MeasureText(task_name[i], 10.f)), 10.f);
							start_of(collect, &text_dest);
							center(collect, &text_dest, 0);
							center(collect, &text_dest, 1);

							Vector4 back = {528, selected == i ? 132.f : 0.f, 132, 132};
							draw_texture_v2(back, xyv4(collect));
							draw_text(xyv4(text_dest), task_name[i], 10);
						}
						
						Vector4 confirm = v4zw(100, 25);
						start_of(dest, &confirm);
						bottom_of(dest, &confirm);
						center(tasks, &confirm, 0);
						pad(&confirm, BOTTOM, 10);

						bool can_click = true;
						if (selected == 1) {
							can_click = state->thing_data->food_amt - 200 > 0 && state->thing_data->worker_amt - 10 > 0;	
						} else if(selected == 2) {
							can_click = state->thing_data->food_amt - state->thing_data->worker_amt > 0; 
						}

						if(ui_btn(xyv4(confirm), "Confirm", 10, can_click)) {
							PlaySound(ui_click);
							ThingData* data = (ThingData*)state->player->user_data;
							switch(selected) {
								case 0:
									data->current_task = TASK_COLLECT;
									state->show_thing_ui = false;
									break;
								case 1:
									data->current_task = TASK_DEFENSE;
									state->show_thing_ui = false;
									break;
								case 2:
									data->current_task = TASK_REPRODUCE;
							}
							data->last_worker_amt = data->worker_amt;
						}

						Vector4 other = {dest.x + 447, dest.y, 128, dest.w};
						int to_switch = selected != hover ? hover : selected;
						switch (to_switch) {
							case 0: // COLLECT
							{
								Vector4 title = v4zw(other.z, 10);
								start_of(other, &title);
								pad(&title, LEFT, 10);
								pad(&title, TOP, 10);

								draw_text(xyv4(title), "Info:", 10);

								Vector4 food_icon_dest = v4zw(32, 32);
								start_of(other, &food_icon_dest);
								below(title, &food_icon_dest);
								center(other, &food_icon_dest, 0);
								pad(&food_icon_dest, TOP, 3);
								
								draw_texture_v2(FOOD_ICON, xyv4(food_icon_dest));

								const char* food_cost_str = TextFormat("-%d/+~%d", state->thing_data->worker_amt, state->thing_data->worker_amt * 4);
								float size = MeasureText(food_cost_str, 10);
								Vector4 food_cost = v4zw(size, 10);
								start_of(other, &food_cost);
								below(food_icon_dest, &food_cost);
								center(other, &food_cost, 0);

								draw_text(xyv4(food_cost), food_cost_str, 10);

							}
							break;
							case 1: // Defense
							{
								Vector4 title = v4zw(other.z, 10);
								start_of(other, &title);
								pad(&title, LEFT, 10);
								pad(&title, TOP, 10);

								draw_text(xyv4(title), "Info:", 10);

								Vector4 food_icon_dest = v4zw(32, 32);
								start_of(other, &food_icon_dest);
								below(title, &food_icon_dest);
								center(other, &food_icon_dest, 0);
								pad(&food_icon_dest, TOP, 3);
								
								draw_texture_v2(FOOD_ICON, xyv4(food_icon_dest));

								const char* food_cost_str = TextFormat("-%d", 200);
								float size = MeasureText(food_cost_str, 10);
								Vector4 food_cost = v4zw(size, 10);
								start_of(other, &food_cost);
								below(food_icon_dest, &food_cost);
								center(other, &food_cost, 0);

								draw_text(xyv4(food_cost), food_cost_str, 10);
								
								Vector4 worker_icon_dest = v4zw(32, 32);
								start_of(other, &worker_icon_dest);
								below(food_cost, &worker_icon_dest);
								center(other, &worker_icon_dest, 0);
								pad(&worker_icon_dest, TOP, 3);
							
								draw_texture_v2(WORKER_ICON, xyv4(worker_icon_dest));

								const char* worker_cost_str = TextFormat("-%d", 10);
								size = MeasureText(worker_cost_str, 10);
								Vector4 worker_cost = v4zw(size, 10);
								start_of(other, &worker_cost);
								below(worker_icon_dest, &worker_cost);
								center(other, &worker_cost, 0);

								draw_text(xyv4(worker_cost), worker_cost_str, 10);
							}
							break;
							case 2:
							{
								Vector4 title = v4zw(other.z, 10);
								start_of(other, &title);
								pad(&title, LEFT, 10);
								pad(&title, TOP, 10);

								draw_text(xyv4(title), "Info:", 10);

								Vector4 food_icon_dest = v4zw(32, 32);
								start_of(other, &food_icon_dest);
								below(title, &food_icon_dest);
								center(other, &food_icon_dest, 0);
								pad(&food_icon_dest, TOP, 3);
								
								draw_texture_v2(FOOD_ICON, xyv4(food_icon_dest));

								const char* food_cost_str = TextFormat("-%d", state->thing_data->worker_amt);
								float size = MeasureText(food_cost_str, 10);
								Vector4 food_cost = v4zw(size, 10);
								start_of(other, &food_cost);
								below(food_icon_dest, &food_cost);
								center(other, &food_cost, 0);

								draw_text(xyv4(food_cost), food_cost_str, 10);
								
								Vector4 worker_icon_dest = v4zw(32, 32);
								start_of(other, &worker_icon_dest);
								below(food_cost, &worker_icon_dest);
								center(other, &worker_icon_dest, 0);
								pad(&worker_icon_dest, TOP, 3);
							
								draw_texture_v2(WORKER_ICON, xyv4(worker_icon_dest));

								const char* worker_cost_str = TextFormat("+%d", state->thing_data->worker_amt / 2);
								size = MeasureText(worker_cost_str, 10);
								Vector4 worker_cost = v4zw(size, 10);
								start_of(other, &worker_cost);
								below(worker_icon_dest, &worker_cost);
								center(other, &worker_cost, 0);

								draw_text(xyv4(worker_cost), worker_cost_str, 10);
							}
							break;
						}
					}
				}

				// :hud
				{
					push_layer(L_HUD);
					{
						Vector4 dest = v4(0, 0, RENDER_SIZE.x, RENDER_SIZE.y);
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

						draw_texture_v2(FOOD_ICON, xyv4(food_dest));
						draw_text(xyv4(food_amt), foodstr, 20);
						draw_texture_v2(WORKER_ICON, xyv4(workers_dest));
						draw_text(xyv4(worker_amt), workerstr, 20);

						if (state->thing_data->current_task != TASK_NONE && state->time_for_predator > 30) {
							Vector4 skip_btn = v4zw(90, 32);
							bottom_of(dest, &skip_btn);
							center(dest, &skip_btn, 0);
							pad(&skip_btn, BOTTOM, 10);

							if (ui_btn(xyv4(skip_btn), "Skip..", 10)) {
								PlaySound(ui_click);
								state->dt_speed = 10;
							}
							
						}

						if(state->time_for_predator > 0) {
						Time t = seconds_to_hm(state->time_for_predator);

						char buf[1024] = {0};
						std::snprintf(buf, 1024, "%02d:%02d:%02d", t.h, t.m, t.s);

						Vector4 predators_time = v4zw((float)MeasureText(buf, 20), 20);
						end_of(dest, &predators_time);
						pad(&predators_time, TOP, 10);
						pad(&predators_time, RIGHT, predators_time.z + 10);

						Color color = WHITE;
						if (state->show_thing_ui)
							color = ColorAlpha(WHITE, ((sinf(GetTime() * 3) * .5) + .5));

						draw_text(xyv4(predators_time),buf, 20, color);
						} else if(state->predator->health > 0) {
							char buf[1024] = {0};
							std::snprintf(buf, 1024, "%04d/%d", state->predator->health, PREDATOR_HP);
							
							Vector4 predator_health = v4zw((float)MeasureText(buf, 20), 20);
							center(dest, &predator_health, 0);
							pad(&predator_health, TOP, 10);

							draw_text(xyv4(predator_health), buf, 20);
					
						}

						
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
							"and reproduce.",
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

						float last_y = 0.f;
						for (int i = 0; i < message_len; i++) {
							float message_sz = MeasureText(messages[i], 10);
							Vector4 message_dest = v4zw(message_sz, 10);
							start_of(dest, &message_dest);
							below(title_dest, &message_dest);
							pad(&message_dest, TOP, 10);
							center(dest, &message_dest, 0);
							message_dest.y += i * message_dest.w;
							draw_text(xyv4(message_dest), messages[i], 10);
							last_y = message_dest.y + message_dest.w;
						}
						
						size = MeasureText("Icons:", 20);
						Vector4 icon_dest = v4zw(size, 20);
						start_of(dest, &icon_dest);
						icon_dest.y = last_y;
						center(dest, &icon_dest, 0);
						pad(&icon_dest, TOP, 10);

						draw_text(xyv4(icon_dest), "Icons:", 20);

						Vector4 icon_food = v4zw(32, 32);
						start_of(dest, &icon_food);
						below(icon_dest, &icon_food);
						pad(&icon_food, LEFT, 10);

						draw_texture_v2(FOOD_ICON, xyv4(icon_food));
						
						Vector4 icon_food_label = v4zw(float(MeasureText("Food", 10)), 10);
						end_of(icon_food, &icon_food_label);
						center(icon_food, &icon_food_label, 1);

						draw_text(xyv4(icon_food_label), "Food", 10);

						Vector4 icon_worker = v4zw(32, 32);
						end_of(dest, &icon_worker);
						below(icon_dest, &icon_worker);
						pad(&icon_worker, RIGHT, icon_worker.z + 10);

						draw_texture_v2(WORKER_ICON, xyv4(icon_worker));

						Vector4 icon_worker_label = v4zw((float)MeasureText("Ant", 10), 10);
						start_of(icon_worker, &icon_worker_label);
						pad(&icon_worker_label, RIGHT, icon_worker_label.z);
						center(icon_worker, &icon_worker_label, 1);

						draw_text(xyv4(icon_worker_label), "Ant", 10);
						
						if (ui_btn(xyv4(ok_btn), "Start", 10)) {
							state->show_begin_message = false;
							PlaySound(ui_click);
							state->show_thing_ui = true;
						}
				}
			}

			if (state->lost) {
				StopMusicStream(music);

				Vector4 dest =  v4v2(ZERO, RENDER_SIZE);
				draw_quad(dest, ColorAlpha(BLACK, .5));

				Vector4 text = v4zw(float(MeasureText("You Lost...", 40)), 40);
				center(dest, &text, 0);
				center(dest, &text, 1);

				draw_text(xyv4(text), "You Lost...", 40);
			 } else if(state->win) {
			 	StopMusicStream(music);

				Vector4 dest =  v4v2(ZERO, RENDER_SIZE);
				draw_quad(dest, ColorAlpha(BLACK, .5));

				Vector4 text = v4zw(float(MeasureText("You Win!!!", 40)), 40);
				center(dest, &text, 0);
				center(dest, &text, 1);

				draw_text(xyv4(text), "You Win...", 40);
			 }
			
			flush_renderer();

		}
		EndTextureMode();

		RenderTexture2D final = ui_texture;
		
		
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

int main(void) {

	// :raylib
	SetTraceLogLevel(LOG_WARNING);
	InitWindow(WINDOW_SIZE.x, WINDOW_SIZE.y, "ld56");
	InitAudioDevice();
	SetTargetFPS(60);
	SetExitKey(KEY_Q);
	
	// :load
	Texture2D atlas = LoadTexture("./res/atlas.png");
	game_texture = LoadRenderTexture(RENDER_SIZE.x, RENDER_SIZE.y);
	light_texture = LoadRenderTexture(RENDER_SIZE.x, RENDER_SIZE.y);
	ui_texture = LoadRenderTexture(RENDER_SIZE.x, RENDER_SIZE.y);
	ui_click = LoadSound("./res/btn_click.wav");
	loop_1 = LoadMusicStream("./res/loop_1.ogg");
	predator_music = LoadMusicStream("./res/predator.ogg");
	Sound remove_flower = LoadSound("./res/remove_flower.wav");
	hover_sound = LoadSound("./res/hover.wav");
	Sound shoot = LoadSound("./res/shoot.wav");
	Sound died = LoadSound("./res/died.wav");

	// :init
	renderer = (Renderer*)arena_alloc(&arena, sizeof(Renderer));
	memset(renderer->layers, 0, sizeof(RenderLayer) * MAX_LAYERS);
	renderer->layer_stack = {0};
	renderer->current_layer = 0;
	renderer->atlas = atlas;
	
	state = (State*)arena_alloc(&arena, sizeof(State));
	memset(state->entities, 0, sizeof(Entity) * MAX_ENTITIES);
	state->dt_speed = 1;	
	state->cam = Camera2D{};
	state->cam.zoom = 1.f;
	state->cam.offset = RENDER_SIZE / v2of(2);
	state->show_begin_message = true;
	state->time_for_predator = 600;
	state->remove_flower = remove_flower;
	state->shoot = shoot;
	state->died = died;
	state->lost = false;
	state->win = false;

	Vector2 player_size = v2(48, 64);
	player_pos = ZERO - (player_size / 2);
	state->player = en_thing(player_pos, player_size);
	state->thing_data = (ThingData*)state->player->user_data; 

	assert(renderer != NULL && "arena returned null");

	flower_spawn_time = .8;
	
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
	state->flower_cnt = 256;

	Music music = loop_1;
	PlayMusicStream(music);

	float volume = 0;
	bool in_predator = false;
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(update_frame, 60, 1);
#else
    while (!WindowShouldClose()) {
    	update_frame();
	}
#endif

	CloseWindow();

	return 0;
}
