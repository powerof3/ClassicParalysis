#include "Hooks.h"
#include "Settings.h"

namespace ClassicParalysis
{
	void detail::clear_actor_velocity(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return;
		}

		if (const auto charController = a_actor->GetCharController()) {
			charController->SetLinearVelocityImpl(0.0f);
			charController->initialVelocity = 0.0f;
			charController->velocityTime = 0.0f;
		}
	}

	void detail::freeze(RE::Actor* a_actor)
	{
		a_actor->StopCurrentDialogue();
		a_actor->InterruptCast(false);
		a_actor->StopInteractingQuick(true);

		if (const auto currentProcess = a_actor->currentProcess) {
			currentProcess->ClearMuzzleFlashes();
		}

		a_actor->boolFlags.reset(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = a_actor->GetCharController()) {
			charController->flags.set(RE::CHARACTER_FLAGS::kNotPushable, RE::CHARACTER_FLAGS::kNoCharacterCollisions);
			charController->flags.reset(RE::CHARACTER_FLAGS::kRecordHits, RE::CHARACTER_FLAGS::kHitFlags, RE::CHARACTER_FLAGS::kHitDamage);
		}

		clear_actor_velocity(a_actor);
		a_actor->EnableAI(false);
		a_actor->StopMoving(1.0f);
	}

	void detail::unfreeze(RE::Actor* a_actor)
	{
		a_actor->boolFlags.set(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = a_actor->GetCharController()) {
			charController->flags.reset(RE::CHARACTER_FLAGS::kNotPushable, RE::CHARACTER_FLAGS::kNoCharacterCollisions);
			charController->flags.set(RE::CHARACTER_FLAGS::kRecordHits, RE::CHARACTER_FLAGS::kHitFlags, RE::CHARACTER_FLAGS::kHitDamage);
		}

		clear_actor_velocity(a_actor);
		a_actor->EnableAI(true);
	}

	bool detail::do_not_freeze(RE::Actor* a_actor)
	{
		return a_actor->GetFormID() == 0x000D7505  // Saadia
		       || a_actor->HasKeywordString("ClassicParalysis_NoFreeze");
	}

	bool detail::is_invalid_effect(RE::ParalysisEffect* a_this)
	{
		if (const auto spell = a_this->spell) {
			static auto frostKeyword = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("MagicDamageFrost");

			const auto settings = Settings::GetSingleton();
			if ((spell->HasKeyword(frostKeyword) && !settings->frostSpells) || !settings->paralysisSpells) {
				return true;
			}
		}
		return false;
	}

	namespace Effect
	{
		struct Start
		{
			static void thunk(RE::ParalysisEffect* a_this)
			{
				if (detail::is_invalid_effect(a_this)) {
					return func(a_this);
				}

				const auto target = a_this->target;
				const auto ref = target ? target->GetTargetStatsObject() : nullptr;
				const auto actor = ref ? ref->As<RE::Actor>() : nullptr;

				if (actor) {
					if (detail::do_not_freeze(actor)) {
						func(a_this);
					} else if (!actor->IsDead()) {
						if (!actor->IsPlayerRef()) {
							detail::freeze(actor);
						} else {
							actor->SetLifeState(RE::ACTOR_LIFE_STATE::kRestrained);
						}
					}
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static inline constexpr std::size_t            idx = 0x14;
		};

		struct Finish
		{
			static void thunk(RE::ParalysisEffect* a_this)
			{
				if (detail::is_invalid_effect(a_this)) {
					return func(a_this);
				}

				const auto target = a_this->target;
				const auto ref = target ? target->GetTargetStatsObject() : nullptr;
				const auto actor = ref ? ref->As<RE::Actor>() : nullptr;

				if (actor) {
					if (detail::do_not_freeze(actor)) {
						func(a_this);
					} else {
						if (!actor->IsPlayerRef()) {
							detail::unfreeze(actor);
						} else {
							actor->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
						}
						actor->StopMoving(1.0f);
					}
				}
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static inline constexpr std::size_t            idx = 0x15;
		};

		struct Update
		{
			static void thunk(RE::ParalysisEffect* a_this, float a_delta)
			{
				if (detail::is_invalid_effect(a_this)) {
					return func(a_this, a_delta);
				}

				const auto target = a_this->target;
				const auto ref = target ? target->GetTargetStatsObject() : nullptr;
				const auto actor = ref ? ref->As<RE::Actor>() : nullptr;

				if (actor) {
					if (detail::do_not_freeze(actor)) {
						return func(a_this, a_delta);
					} else if (!actor->IsAIEnabled()) {
						if (!actor->IsPlayerRef()) {
							detail::clear_actor_velocity(actor);
						}
					}
				}

				func(a_this, a_delta);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static inline constexpr std::size_t            idx = 0x04;
		};

		void Install()
		{
			stl::write_vfunc<RE::ParalysisEffect, Start>();
			stl::write_vfunc<RE::ParalysisEffect, Finish>();
			stl::write_vfunc<RE::ParalysisEffect, Update>();
		}
	}

	namespace Fixes
	{
		struct CanBePushed
		{
			static bool thunk(RE::Actor* a_actor)
			{
				const auto result = func(a_actor);
				if (result && a_actor && !a_actor->IsAIEnabled()) {
					detail::unfreeze(a_actor);
				}
				return result;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct GetProcessLevel
		{
			static std::int32_t thunk(RE::Actor* a_actor)
			{
				const auto processLevel = func(a_actor);
				if (a_actor && !a_actor->IsAIEnabled()) {
					detail::unfreeze(a_actor);
					return -1;
				}
				return processLevel;
			}
			static inline REL::Relocation<decltype(&thunk)> func;
		};

		void Install()
		{
			REL::Relocation<std::uintptr_t> push_actor_away{ RELOCATION_ID(38858, 39895), OFFSET(0x7E, 0x68) };
			stl::write_thunk_call<CanBePushed>(push_actor_away.address());

			REL::Relocation<std::uintptr_t> start_kill_move{ RELOCATION_ID(37659, 38613), 0x10 };
			stl::write_thunk_call<GetProcessLevel>(start_kill_move.address());
		}
	}

	void Install()
	{
		Settings::GetSingleton()->Load();

		Effect::Install();
		Fixes::Install();
	}
}
