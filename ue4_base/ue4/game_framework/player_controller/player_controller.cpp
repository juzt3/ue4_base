#include <includes.h>

bool ue4::game_framework::a_player_controller::world_to_screen(const ue4::math::vector& world, ue4::math::vector_2d& screen) {
	struct {
		ue4::math::vector world;
		ue4::math::vector_2d screen;
		bool player_viewport_relative;
		bool return_value;
	} params{};

	params.world = world;
	params.screen = screen;
	params.player_viewport_relative = false;

	ue4::sdk::process_event(this, ue4::sdk::world_to_screen, &params);

	screen = params.screen;

	return params.return_value;
}

void ue4::game_framework::a_player_controller::move_to_location_order(const ue4::math::vector& from, const ue4::math::vector& to) {
	struct {
		ue4::math::vector from;
		ue4::math::vector to;
	} params{};

	params.from = from;
	params.to = to;

	ue4::sdk::process_event(this, ue4::sdk::move_to_location_order, &params);
}

void ue4::game_framework::a_player_controller::set_pointer(const ue4::math::vector& location) {
	struct {
		ue4::math::vector location;
	} params{};

	params.location = location;

	ue4::sdk::process_event(this, ue4::sdk::set_pointer, &params);
}

void ue4::game_framework::a_player_controller::move_to_location(const ue4::math::vector& from, const ue4::math::vector& to, bool is_by_mouse) {
	struct {
		ue4::math::vector from;
		ue4::math::vector to;
		bool is_by_mouse;
	} params{};

	params.from = from;
	params.to = to;
	params.is_by_mouse = is_by_mouse;

	ue4::sdk::process_event(this, ue4::sdk::move_to_location, &params);
}

void ue4::game_framework::a_player_controller::teleport_to_location(const ue4::math::vector& location) {
	struct {
		ue4::math::vector location;
	} params{};

	params.location = location;

	ue4::sdk::process_event(this, ue4::sdk::teleport_to_location, &params);
}

void ue4::game_framework::a_player_controller::use_command(const ue4::containers::f_string& cmd) {
	struct {
		ue4::containers::f_string cmd;
	} params{};

	params.cmd = cmd;

	ue4::sdk::process_event(this, ue4::sdk::use_command, &params);
}