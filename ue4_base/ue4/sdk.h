#pragma once

namespace ue4::core_object {
	class u_object;
}

namespace ue4::sdk {
	// u_canvas
	inline ue4::core_object::u_object* font;
	inline ue4::core_object::u_object* draw_box;
	inline ue4::core_object::u_object* draw_line;
	inline ue4::core_object::u_object* draw_text;
	inline ue4::core_object::u_object* draw_polygon;
	inline ue4::core_object::u_object* text_size;

	// a_player_controller
	inline ue4::core_object::u_object* world_to_screen;
	inline ue4::core_object::u_object* move_to_location_order;
	inline ue4::core_object::u_object* set_pointer;
	inline ue4::core_object::u_object* use_command;
	inline ue4::core_object::u_object* use_skill;
	inline ue4::core_object::u_object* attack;
	inline ue4::core_object::u_object* select_target;

	// alu4_client_controller
	inline ue4::core_object::u_object* move_to_location;
	inline ue4::core_object::u_object* teleport_to_location;
	inline ue4::core_object::u_object* target_attack;

	// LU4ScriptCommands
	inline ue4::core_object::u_object* use_command_by_name;

	// a_actor
	inline ue4::core_object::u_object* get_actor_location;
	inline ue4::core_object::u_object* get_actor_rotation;
	inline ue4::core_object::u_object* get_actor_bounds;
	inline ue4::core_object::u_object* get_distance_to;

	// u_skeletal_mesh_component
	inline std::uintptr_t bone_matrix;
	inline ue4::core_object::u_object* get_bone_name;

	bool initialize();

	void process_event(void* object, void* u_function, void* params);
}
