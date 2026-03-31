#include <includes.h>
#include "../hooks.h"

void __stdcall hooks::post_render::hook(ue4::core_object::u_object* viewport_client, ue4::engine::u_canvas* canvas) {
	render::canvas = canvas;

	const auto world = *reinterpret_cast<decltype(ue4::engine::world)*>(ue4::engine::world);
	if (!world) return;

	const auto local_player = world->owning_game_instance->local_players[0];
	if (!local_player) return;

	const auto player_controller = local_player->player_controller;
	if (!player_controller) return;

	const auto my_player = player_controller->acknowledged_pawn;
	if (!my_player) return;

	// Obtener posición actual del jugador
	ue4::math::vector inicio = my_player->get_location();
	
	// Crear destino CERCANO al jugador (100 unidades adelante en X)
	ue4::math::vector destino;
	destino.x = inicio.x + 300.0f;  // 100 unidades adelante
	destino.y = inicio.y;           // Misma posición Y
	destino.z = inicio.z;           // Misma altura Z
	
	// Solo mover cuando se presiona la tecla T (evita llamadas constantes)
	if (GetAsyncKeyState('T') & 1) {
		render::text({ 100.f, 100.f }, L"Moviendo personaje...", { 0.f, 1.f, 0.f, 1.f });
		player_controller->move_to_location(inicio, destino, true);
		player_controller->set_pointer(destino);
	}

	if (GetAsyncKeyState('C') & 1) {
		render::text({ 100.f, 100.f }, L"Enviando comando...", { 0.f, 1.f, 0.f, 1.f });

		const wchar_t* cmd = L"/useskill Spoil";
		player_controller->use_command(cmd);
	}
	
	// Mostrar posición actual en pantalla
	wchar_t pos_text[256];
	swprintf(pos_text, 256, L"Pos: X=%.1f Y=%.1f Z=%.1f", inicio.x, inicio.y, inicio.z);
	render::text({ 130.f, 120.f }, pos_text, { 1.f, 1.f, 0.f, 1.f });

	/*
	auto actors = world->persistent_level->actors;
	for (auto i = 0; i < actors.size(); i++) {
		const auto actor = actors[i];
		if (!actor || !actor->root_component) continue;
		if (actor == my_player) continue;

		const auto get_name = actor->get_name();
		const auto name = std::wstring(get_name.begin(), get_name.end());

		const auto location = actor->get_location();

		ue4::math::vector_2d position{};
		if (player_controller->world_to_screen(location, position)) {
			render::text(position, name.c_str(), { 1.f, 1.f, 1.f, 1.f });
		}
	}
	*/

	hooks::post_render::original(viewport_client, canvas);
}