#pragma once

namespace ue4::game_framework {
	class a_player_controller : public ue4::game_framework::a_controller {
	public:
		char pad_0001[0x68]; // 0x230(0x68)
		ue4::engine::u_player* player; // 0x298(0x08)
		ue4::game_framework::a_pawn* acknowledged_pawn; // 0x2a0(0x08)

		bool world_to_screen(const ue4::math::vector& world, ue4::math::vector_2d& screen);
		void move_to_location_order(const ue4::math::vector& from, const ue4::math::vector& to);
		void set_pointer(const ue4::math::vector& location);
		void move_to_location(const ue4::math::vector& from, const ue4::math::vector& to, bool is_by_mouse);
		void teleport_to_location(const ue4::math::vector& location);
		void use_command(const ue4::containers::f_string& cmd);
		void use_skill(const int32_t Skill_ID);
		void attack(bool Force_0, bool bLockMovement_0);
		void select_target(class AActor* Actor, bool ForceAttack, bool* Selection);
	};
}