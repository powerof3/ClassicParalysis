#pragma once

namespace ClassicParalysis
{
	namespace detail
	{
		void clear_actor_velocity(RE::Actor* a_actor);
		void freeze(RE::Actor* a_actor);
		void unfreeze(RE::Actor* a_actor);
		bool do_not_freeze(RE::Actor* a_actor);
		bool is_invalid_effect(RE::ParalysisEffect* a_this);
	}

	namespace Effect
	{
		void Install();
	}

	namespace Fixes
	{
		void Install();
	}

	void Install();
}
