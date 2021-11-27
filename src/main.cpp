#include "Settings.h"

struct detail
{
	static void freeze(RE::Actor& a_actor)
	{
		a_actor.PauseCurrentDialogue();

		a_actor.InterruptCast(false);
		a_actor.StopInteractingQuick(true);

		const auto currentProcess = a_actor.currentProcess;
		if (currentProcess) {
			currentProcess->ClearMuzzleFlashes();
		}

		a_actor.boolFlags.reset(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = a_actor.GetCharController(); charController) {
			charController->flags.set(RE::CHARACTER_FLAGS::kNotPushable);

			charController->flags.reset(RE::CHARACTER_FLAGS::kRecordHits);
			charController->flags.reset(RE::CHARACTER_FLAGS::kHitFlags);
		}

		a_actor.EnableAI(false);

		a_actor.StopMoving(1.0f);
	}

	static void unfreeze(RE::Actor& a_actor)
	{
		a_actor.boolFlags.set(RE::Actor::BOOL_FLAGS::kShouldAnimGraphUpdate);

		if (const auto charController = a_actor.GetCharController(); charController) {
			charController->flags.reset(RE::CHARACTER_FLAGS::kNotPushable);

			charController->flags.set(RE::CHARACTER_FLAGS::kRecordHits);
			charController->flags.set(RE::CHARACTER_FLAGS::kHitFlags);
		}

		a_actor.EnableAI(true);
	}

	static bool is_invalid_effect(RE::ParalysisEffect& a_this)
	{
		const auto mgef = a_this.GetBaseObject();
		if (mgef) {
			const auto settings = Settings::GetSingleton();
			if (mgef->HasKeywordString("MagicDamageFrost") && !settings->frostSpells || !settings->paralysisSpells) {
				return true;
			}
		}
		return false;
	}
};

namespace Paralysis
{

	static inline RE::FormID SaadiaID{ 0x000D7505 };
	
	struct Start
	{
		static void thunk(RE::ParalysisEffect* a_this)
		{
			if (detail::is_invalid_effect(*a_this)) {
				func(a_this);
					
				return;
			}

			const auto target = a_this->target;
			const auto ref = target ? target->GetTargetStatsObject() : nullptr;
			const auto actor = ref ? ref->As<RE::Actor>() : nullptr;

			if (actor) {
				if (actor->GetFormID() == SaadiaID) {
					func(a_this);
				} else if (!actor->IsDead()) {
					if (!actor->IsPlayerRef()) {
						detail::freeze(*actor);
					} else {
						actor->SetLifeState(RE::ACTOR_LIFE_STATE::kRestrained);
					}
				}
			}
		}
		static inline REL::Relocation<decltype(&thunk)> func;
	};

	struct Finish
	{
		static void thunk(RE::ParalysisEffect* a_this)
		{
			if (detail::is_invalid_effect(*a_this)) {
				func(a_this);

				return;
			}
			
			const auto target = a_this->target;
			const auto ref = target ? target->GetTargetStatsObject() : nullptr;
			const auto actor = ref ? ref->As<RE::Actor>() : nullptr;

			if (actor) {
				if (actor->GetFormID() == SaadiaID) {
					func(a_this);
				} else {
					if (!actor->IsPlayerRef()) {
						detail::unfreeze(*actor);
					} else {
						actor->SetLifeState(RE::ACTOR_LIFE_STATE::kAlive);
					}
					actor->StopMoving(1.0f);
				}
			}
		}
		static inline REL::Relocation<decltype(&thunk)> func;
	};

	void Install()
	{
		stl::write_vfunc<RE::ParalysisEffect, 0x14, Start>();
		stl::write_vfunc<RE::ParalysisEffect, 0x15, Finish>();
	}
}

namespace Paralysis::Fixes
{
	struct CanBePushed
	{
		static bool thunk(RE::Actor* a_actor)
		{
			const auto result = func(a_actor);
			if (result && a_actor && !a_actor->IsAIEnabled()) {
				detail::unfreeze(*a_actor);
			}
			return result;
		}
		static inline REL::Relocation<decltype(&thunk)> func;
	};

	struct GetProcessLevel
	{
		static std::int32_t thunk(RE::Actor* a_actor)
		{
			const auto processLevel = func(a_actor);
			if (a_actor && !a_actor->IsAIEnabled()) {
				detail::unfreeze(*a_actor);
				return -1;
			}
			return processLevel;
		}
		static inline REL::Relocation<decltype(&thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> push_actor_away{ REL::ID(39895) };
		stl::write_thunk_call<CanBePushed>(push_actor_away.address() + 0x68);

#if 0
		REL::Relocation<std::uintptr_t> stagger_actor{ REL::ID(38858) };
		stl::write_thunk_call<CanBePushed>(stagger_actor.address() + 0x74);
#endif

		REL::Relocation<std::uintptr_t> start_kill_move{ REL::ID(38613) };
		stl::write_thunk_call<GetProcessLevel>(start_kill_move.address() + 0x10);
	}
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Classic Paralysis");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	
	logger::info("loaded plugin");

	SKSE::Init(a_skse);
	SKSE::AllocTrampoline(28);

	Settings::GetSingleton()->Load();
	
	Paralysis::Install();
	Paralysis::Fixes::Install();

	return true;
}
