#include <includes.h>
#include "hooks.h"

bool hooks::initialize() {
	const auto world = *reinterpret_cast<decltype(ue4::engine::world)*>(ue4::engine::world);
	if (!world) return false;

	const auto local_player = world->owning_game_instance->local_players[0];
	if (!local_player) return false;

	const auto viewport_client = local_player->viewport_client;
	if (!viewport_client) return false;

	void** viewport_client_vtable = viewport_client->vf_table;
	if (!viewport_client_vtable) return false;

	if (MH_Initialize() != MH_OK) throw std::runtime_error("failed to initialize min_hook");

	if (MH_CreateHook(viewport_client_vtable[hooks::post_render::index], &hooks::post_render::hook, reinterpret_cast<void**>(&hooks::post_render::original)) != MH_OK) {
		throw std::runtime_error("failed to hook post_render");
	}

	// Hook del game thread usando UWorld::Tick (mas confiable)
	// UWorld::Tick esta en index 0x48 en UE4 4.26
	// Cast a u_object para obtener la vtable (UWorld hereda de UObject)
	void** world_vtable = reinterpret_cast<ue4::core_object::u_object*>(world)->vf_table;
	if (world_vtable) {
		std::cout << "[hooks] UWorld vtable encontrada: " << world_vtable << std::endl;
		// Debug: mostrar varias funciones de la vtable para encontrar Tick
		std::cout << "[hooks] vtable[0x40]=" << world_vtable[0x40] << std::endl;
		std::cout << "[hooks] vtable[0x48]=" << world_vtable[0x48] << std::endl;
		std::cout << "[hooks] vtable[0x50]=" << world_vtable[0x50] << std::endl;
		std::cout << "[hooks] vtable[0x58]=" << world_vtable[0x58] << std::endl;
		std::cout << "[hooks] Intentando hook UWorld::Tick en index 0x48" << std::endl;
		
		// UWorld::Tick tiene indice 0x48 tipicamente
		if (MH_CreateHook(world_vtable[0x48], &hooks::game_tick::hook, reinterpret_cast<void**>(&hooks::game_tick::original)) != MH_OK) {
			std::cout << "[hooks] ERROR: No se pudo crear hook en 0x48" << std::endl;
		} else {
			std::cout << "[hooks] UWorld::Tick hook creado exitosamente!" << std::endl;
		}
	} else {
		std::cout << "[hooks] ERROR: UWorld vtable es null!" << std::endl;
	}

	if (MH_EnableHook(nullptr) != MH_OK) throw std::runtime_error(_("failed to initialize hooks"));

	return true;
}

bool hooks::release() {
	MH_Uninitialize();

	MH_DisableHook(nullptr);
	MH_RemoveHook(nullptr);

	utils::console::release();

	return true;
}