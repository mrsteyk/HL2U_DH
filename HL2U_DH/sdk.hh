#pragma once

#include "pch.h"

#define PI 3.14159265358979323846f
#define DEG2RAD( x ) ( ( float )( x ) * ( float )( ( float )( PI ) / 180.0f ) )
#define RAD2DEG( x ) ( ( float )( x ) * ( float )( 180.0f / ( float )( PI ) ) )
#define clamp(val, min, max) (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

enum playercontrols
{
	IN_ATTACK = (1 << 0),
	IN_JUMP = (1 << 1),
	IN_DUCK = (1 << 2),
	IN_FORWARD = (1 << 3),
	IN_BACK = (1 << 4),
	IN_USE = (1 << 5),
	IN_CANCEL = (1 << 6),
	IN_LEFT = (1 << 7),
	IN_RIGHT = (1 << 8),
	IN_MOVELEFT = (1 << 9),
	IN_MOVERIGHT = (1 << 10),
	IN_ATTACK2 = (1 << 11),
	IN_RUN = (1 << 12),
	IN_RELOAD = (1 << 13),
	IN_ALT1 = (1 << 14),
	IN_ALT2 = (1 << 15),
	IN_SCORE = (1 << 16),	// Used by client.dll for when scoreboard is held down
	IN_SPEED = (1 << 17),	// Player is holding the speed key
	IN_WALK = (1 << 18),	// Player holding walk key
	IN_ZOOM = (1 << 19),	// Zoom key for HUD zoom
	IN_WEAPON1 = (1 << 20),	// weapon defines these bits
	IN_WEAPON2 = (1 << 21),	// weapon defines these bits
	IN_BULLRUSH = (1 << 22),
};

enum source_lifestates
{
	LIFE_ALIVE,
	LIFE_DYING,
	LIFE_DEAD,
	LIFE_RESPAWNABLE,
	LIFE_DISCARDBODY,
};

class net_channel {
public:
};

class engine_client {
public:
	net_channel* get_net_channel() {
		return_virtual_func(get_net_channel, 74, 0);
	}
	u32 local_player_index() {
		return_virtual_func(local_player_index, 12, 0);
	}
	bool in_game() {
		return_virtual_func(in_game, 25, 0);
	}
};

class ConCommandBase;

class Cvar {
public:
	Cvar() = delete;

	u32 allocate_dll_identifier() {
		return_virtual_func(allocate_dll_identifier, 5, 0);
	}

	void register_command(ConCommandBase* command) {
		return_virtual_func(register_command, 6, 0, command);
	}

	void unregister_command(ConCommandBase* command) {
		return_virtual_func(unregister_command, 7, 0, command);
	}

	ConCommandBase* root_node() {
		return_virtual_func(root_node, 16, 0);
	}

	ConCommandBase* find_var(const char* name) {
		return_virtual_func(find_var, 12, 0, name);
	}
};

struct EntityHandle {
	u32 serial_index;
};

class Entity {
public:
	Entity() = delete;

	// helper functions
	bool is_valid();

	EntityHandle& to_handle();

	template <typename T, u32 offset>
	auto set(T data) { *reinterpret_cast<T*>(reinterpret_cast<uptr>(this) + offset) = data; }

	template <typename T>
	auto set(u32 offset, T data) { *reinterpret_cast<T*>(reinterpret_cast<uptr>(this) + offset) = data; }

	template <typename T, u32 offset>
	auto& get() { return *reinterpret_cast<T*>(reinterpret_cast<uptr>(this) + offset); }

	template <typename T>
	auto& get(u32 offset) { return *reinterpret_cast<T*>(reinterpret_cast<uptr>(this) + offset); }

	// upcasts
	class Player* to_player();
	class Weapon* to_weapon();

	// virtual functions
	struct ClientClass* client_class();

	bool dormant();
	u32  index();
};

class EntList {
public:
	EntList() = delete;

	Entity* entity(u32 index) {
		return_virtual_func(entity, 3, 0, index);
	}

	Entity* from_handle(EntityHandle h) {
		return_virtual_func(from_handle, 4, 0, h);
	}

	u32 max_entity_index() {
		return_virtual_func(max_entity_index, 6, 0);
	}

	class EntityRange {
		EntList* parent;

		u32 max_entity;

	public:
		class Iterator {
			u32      index;
			EntList* parent;

		public:
			// TODO: should we use 1 here or should we use 0 and force people
			// that loop to deal with that themselves...
			Iterator(EntList* parent) : index(1), parent(parent) {}
			explicit Iterator(u32 index, EntList* parent)
				: index(index), parent(parent) {}

			auto& operator++() {
				++index;
				return *this;
			}

			auto operator*() {
				return parent->entity(index);
			}

			auto operator==(const Iterator& b) {
				return index == b.index;
			}

			auto operator!=(const Iterator& b) {
				return !(*this == b);
			}
		};

		EntityRange(EntList* parent) : parent(parent), max_entity(parent->max_entity_index()) {}

		explicit EntityRange(EntList* parent, u32 max_entity) : parent(parent), max_entity(max_entity) {}

		auto begin() { return Iterator(parent); }

		auto end() { return Iterator(max_entity, parent); }
	};

	auto get_range() { return EntityRange(this); }
	auto get_range(u32 max_entity) { return EntityRange(this, max_entity + 1); }

};

static float normalize_yaw(float ang)
{
	while (ang < -180.0f)
		ang += 360.0f;
	while (ang > 180.0f)
		ang -= 360.0f;
	return ang;
}