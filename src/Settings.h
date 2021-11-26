#pragma once

class Settings
{
public:
	[[nodiscard]] static Settings* GetSingleton()
	{
		static Settings singleton;
		return std::addressof(singleton);
	}

	void Load()
	{				
		constexpr auto path = L"Data/SKSE/Plugins/po3_ClassicParalysis.ini";

		CSimpleIniA ini;
		ini.SetUnicode();

		ini.LoadFile(path);

		detail::get_value(ini, frostSpells, "Settings", "Frost Spells", ";Enable classic paralysis for frost spells (eg. IceForm)");
		detail::get_value(ini, paralysisSpells, "Settings", "Paralysis Spells", ";Enable classic paralysis for paralysis spells");

		ini.SaveFile(path);
	}

	bool frostSpells{ false };
	bool paralysisSpells{ true };

private:
	struct detail
	{
		static void get_value(CSimpleIniA& a_ini, bool& a_value, const char* a_section, const char* a_key, const char* a_comment)
		{
			a_value = a_ini.GetBoolValue(a_section, a_key, a_value);
			a_ini.SetBoolValue(a_section, a_key, a_value, a_comment);
		};
	};
};
