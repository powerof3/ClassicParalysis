#include "version.h"


namespace
{
	void ResetMovementController(RE::Actor* a_actor, float a_delta)
	{
		using func_t = decltype(&ResetMovementController);
		REL::Relocation<func_t> func{ REL::ID(36801) };
		return func(a_actor, a_delta);
	}
}


class Paralysis
{
public:
	static void Hook()
	{
		REL::Relocation<std::uintptr_t> vtbl{ REL::ID(257870) };  //Paralysis vtbl
		_Start = vtbl.write_vfunc(0x014, Start);
		logger::info("Hooked paralysis start.");

		_Stop = vtbl.write_vfunc(0x015, Stop);
		logger::info("Hooked paralysis stop.");
	}

private:
	static void Start(RE::ParalysisEffect* a_this)
	{
		using Flags = RE::CHARACTER_FLAGS;
		using BOOL_BITS = RE::Actor::BOOL_BITS;

		if (const auto target = a_this->target; target) {
			const auto ref = target->GetTargetStatsObject();
			const auto actor = ref ? ref->As<RE::Actor>() : nullptr;
			if (actor && !actor->IsDead()) {
				if (!actor->IsPlayerRef()) {
					actor->PauseCurrentDialogue();
					actor->StopSelectedSpells();
					actor->EnableAI(false);
				} else {
					actor->UpdateLifeState(RE::ACTOR_LIFE_STATE::kRestrained);
				}
				ResetMovementController(actor, 1.0f);
			}
		}
	}
	using Start_t = decltype(&RE::ParalysisEffect::Unk_14);	 // 014
	static inline REL::Relocation<Start_t> _Start;


	static void Stop(RE::ParalysisEffect* a_this)
	{
		using Flags = RE::CHARACTER_FLAGS;
		using BOOL_BITS = RE::Actor::BOOL_BITS;

		if (const auto target = a_this->target; target) {
			const auto ref = target->GetTargetStatsObject();
			const auto actor = ref ? ref->As<RE::Actor>() : nullptr;
			if (actor) {
				if (!actor->IsPlayerRef()) {
					actor->EnableAI(true);
				} else {
					actor->UpdateLifeState(RE::ACTOR_LIFE_STATE::kAlive);
				}
				ResetMovementController(actor, 1.0f);
			}
		}
	}
	using Stop_t = decltype(&RE::ParalysisEffect::Unk_15);	// 015
	static inline REL::Relocation<Stop_t> _Stop;
};


class ParalysisFixes
{
public:
	static void Hook()
	{
		auto& trampoline = SKSE::GetTrampoline();
		REL::Relocation<std::uintptr_t> PushActorAway{ REL::ID(38858) };
		_CanBePushed = trampoline.write_call<5>(PushActorAway.address() + 0x7E, CanBePushed);

		/*REL::Relocation<std::uintptr_t> StaggerActor{ REL::ID(36700) };
		_CanBeStaggered = trampoline.write_call<5>(StaggerActor.address() + 0x74, CanBeStaggered);*/
	}

private:
	static bool CanBePushed(RE::Actor* a_actor)
	{
		const auto result = _CanBePushed(a_actor);
		if (result) {
			if (!a_actor->IsAIEnabled()) {
				a_actor->EnableAI(true);
				ResetMovementController(a_actor, 1.0f);
			}
		}
		return result;
	}
	static inline REL::Relocation<decltype(CanBePushed)> _CanBePushed;


	static bool CanBeStaggered(RE::Actor* a_actor)
	{
		const auto result = _CanBeStaggered(a_actor);
		if (result) {
			if (!a_actor->IsAIEnabled()) {
				a_actor->EnableAI(true);
				ResetMovementController(a_actor, 1.0f);
			}
		}
		return result;
	}
	static inline REL::Relocation<decltype(CanBeStaggered)> _CanBeStaggered;
};


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	try {
		auto path = logger::log_directory().value() / "po3_ClassicParalysis.log";
		auto log = spdlog::basic_logger_mt("global log", path.string(), true);
		log->flush_on(spdlog::level::info);

#ifndef NDEBUG
		log->set_level(spdlog::level::debug);
		log->sinks().push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#else
		log->set_level(spdlog::level::info);

#endif
		set_default_logger(log);
		spdlog::set_pattern("[%H:%M:%S] %v");

		logger::info("po3_ClassicParalysis v{}", VERSION_VERSTRING);

		a_info->infoVersion = SKSE::PluginInfo::kVersion;
		a_info->name = "Classic Paralysis";
		a_info->version = VERSION_MAJOR;

		if (a_skse->IsEditor()) {
			logger::critical("Loaded in editor, marking as incompatible");
			return false;
		}

		const auto ver = a_skse->RuntimeVersion();
		if (ver < SKSE::RUNTIME_1_5_39) {
			logger::critical("Unsupported runtime version {}", ver.string());
			return false;
		}
	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}


extern "C" DLLEXPORT bool APIENTRY SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	try {
		logger::info("po3_ClassicParalysis loaded");

		Init(a_skse);
		SKSE::AllocTrampoline(1 << 5);

		Paralysis::Hook();
		ParalysisFixes::Hook();

	} catch (const std::exception& e) {
		logger::critical(e.what());
		return false;
	} catch (...) {
		logger::critical("caught unknown exception");
		return false;
	}

	return true;
}