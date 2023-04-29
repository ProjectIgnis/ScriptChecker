//  SPDX-FileCopyrightText: 2023 Edoardo Lolletti <edoardo762@gmail.com>
//  SPDX-License-Identifier: MIT
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define CORENAME "ocgcore.dll"
#define LoadCore() LoadLibrary("./ocgcore.dll")
#define CloseLibrary(lib) FreeLibrary(lib)
#define GetFunction(lib, funcname) GetProcAddress(lib, funcname)
using core_t = HMODULE;
#else
#include <dlfcn.h>
#ifdef __linux__
#define CORENAME "./libocgcore.so"
#else
#define CORENAME "./libocgcore.dylib"
#endif
#define LoadCore() dlopen(CORENAME, RTLD_NOW)
#define CloseLibrary(lib) dlclose(lib)
#define GetFunction(lib, funcname) dlsym(lib, funcname)
using core_t = void*;
#endif
#ifdef _MSC_VER
#define unreachable() __assume(0)
#else
#define unreachable() __builtin_unreachable()
#endif

template<typename T, typename T2>
inline T function_cast(T2 ptr) {
	using generic_function_ptr = void (*)(void);
	return reinterpret_cast<T>(reinterpret_cast<generic_function_ptr>(ptr));
}

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ocgapi_types.h"

namespace fs = std::filesystem;

namespace {

static constexpr auto LOCATION_DECK = 0x01;
static constexpr auto POS_FACEDOWN = 0xa;

const char* getLogLevelString(OCG_LogTypes level) {
	switch(level) {
		case OCG_LOG_TYPE_ERROR: return "Error";
		case OCG_LOG_TYPE_FROM_SCRIPT: return "From script";
		case OCG_LOG_TYPE_FOR_DEBUG: return "For debug";
		case OCG_LOG_TYPE_UNDEFINED: return "Undefined";
	}
	unreachable();
}

bool isCardScript(const fs::path filename) {
	return filename.stem().string()[0] == 'c' && filename.extension().string() == ".lua";
}

std::optional<uint32_t> tryParseCode(const fs::path& filename) {
	if(!isCardScript(filename))
		return std::nullopt;
	try {
		return static_cast<uint32_t>(std::stoul(filename.stem().string().substr(1)));
	} catch(...) {
		return std::nullopt;
	}
}

int status_code = EXIT_SUCCESS;

uint32_t loading_card = 0;
std::map<uint32_t, fs::path> scripts;
std::map<std::string, fs::path> non_card_scripts;

using OCG_CreateDuel_t = int(*)(OCG_Duel* duel, OCG_DuelOptions options);
OCG_CreateDuel_t OCG_CreateDuel{nullptr};
using OCG_DestroyDuel_t = void(*)(OCG_Duel duel);
OCG_DestroyDuel_t OCG_DestroyDuel{nullptr};
using OCG_DuelNewCard_t = void(*)(OCG_Duel duel, OCG_NewCardInfo info);
OCG_DuelNewCard_t OCG_DuelNewCard{nullptr};
using OCG_LoadScript_t = int(*)(OCG_Duel duel, const char* buffer, uint32_t length, const char* name);
OCG_LoadScript_t OCG_LoadScript{nullptr};
using OCG_GetVersion_t = void(*)(int* major, int* minor);
OCG_GetVersion_t OCG_GetVersion{nullptr};

OCG_Duel pduel{nullptr};

bool loadScript(const fs::path& path) {
	std::ifstream script{path};
	if(script.fail()) {
		fprintf(stderr, "Failed to open script: %s\n", path.string().data());
		return false;
	}
	std::vector<char> buffer;
	buffer.assign(std::istreambuf_iterator<char>(script), std::istreambuf_iterator<char>());
	return buffer.size() && OCG_LoadScript(pduel, buffer.data(), static_cast<uint32_t>(buffer.size()), path.filename().string().data());
}

constexpr inline bool mustNotParseAsCard(uint32_t code) {
	return (code >= 419 && code <= 422) || code == 151000000;
}

void parseScript(const fs::directory_entry& dir_entry) {
	const auto& filename = dir_entry.path().filename();
	auto code = tryParseCode(filename.string());
	if(code.has_value()) {
		if(!mustNotParseAsCard(code.value())) {
			scripts.emplace(code.value(), dir_entry.path());
			return;
		}
	}
	if(filename.extension().string() == ".lua")
		non_card_scripts.emplace(filename.string(), dir_entry.path());
}

void parseScriptFolder(const char* path) {
	for (auto it = fs::recursive_directory_iterator(path); it != end(it); ++it) {
		if(it.depth() > 1) {
			it.pop();
			continue;
		}
		const auto& dir_entry = *it;
		if(dir_entry.is_directory()) {
			if(dir_entry.path().filename().string()[0] == '.') {
				it.disable_recursion_pending();
				continue;
			}
			printf("Found script folder %s\n", dir_entry.path().string().data());
		} else if(dir_entry.is_regular_file())
			parseScript(dir_entry);
	 }
}

}

#define Error(...) do { fprintf(stderr, __VA_ARGS__); return EXIT_FAILURE; } while(0)

int main(int argc, char* argv[]) {
	if(argc == 1) {
		printf("No foler passed, using the current directory\n");
		parseScriptFolder(".");
	} else {
		for(int i = 1; i < argc; ++i) {
			printf("Passed script folder %s\n", argv[i]);
			parseScriptFolder(argv[i]);
		}
	}
	auto utility = non_card_scripts.find("utility.lua");
	auto constant = non_card_scripts.find("constant.lua");
	if(utility == non_card_scripts.end() || constant == non_card_scripts.end())
		Error("Utility or constant scripts were not found\n");

	std::unique_ptr<std::remove_pointer_t<core_t>, void(*)(core_t)> core{ LoadCore(), [](core_t library) { CloseLibrary(library); } };
	if(core == nullptr)
		Error("Failed to load the core\n");

	OCG_GetVersion = function_cast<OCG_GetVersion_t>(GetFunction(core.get(), "OCG_GetVersion"));
	OCG_CreateDuel = function_cast<OCG_CreateDuel_t>(GetFunction(core.get(), "OCG_CreateDuel"));
	OCG_DuelNewCard = function_cast<OCG_DuelNewCard_t>(GetFunction(core.get(), "OCG_DuelNewCard"));
	OCG_DestroyDuel = function_cast<OCG_DestroyDuel_t>(GetFunction(core.get(), "OCG_DestroyDuel"));
	OCG_LoadScript = function_cast<OCG_LoadScript_t>(GetFunction(core.get(), "OCG_LoadScript"));

	if(OCG_GetVersion == nullptr || OCG_CreateDuel == nullptr || OCG_DuelNewCard == nullptr || OCG_DestroyDuel == nullptr || OCG_LoadScript == nullptr)
		Error("Failed to load the needed functions from the core\n");
	{
		int major, minor;
		OCG_GetVersion(&major, &minor);
		if(major != OCG_VERSION_MAJOR || minor < OCG_VERSION_MINOR)
			Error("Unsupported core version\n");
	}

	OCG_DuelOptions opts{};
	opts.cardReader = []([[maybe_unused]] void* payload, uint32_t code, OCG_CardData* data) {
		data->code = code;
	};
	opts.scriptReader = []([[maybe_unused]] void* payload, [[maybe_unused]] OCG_Duel duel, const char* name) -> int {
		fs::path script_path{};
		if(auto code = tryParseCode(name); code.has_value() && !mustNotParseAsCard(code.value())) {
			auto script = scripts.find(code.value());
			if(script == scripts.end()) {
				if(code.value() != 0)
					status_code = EXIT_FAILURE;
				return 0;
			}
			script_path = script->second;
		} else {
			auto script = non_card_scripts.find(name);
			if(script == non_card_scripts.end()) {
				status_code = EXIT_FAILURE;
				return 0;
			}
			script_path = script->second;
		}
		return loadScript(script_path);
	};
	opts.logHandler = []([[maybe_unused]] void* payload, const char* string, int type) {
		status_code = EXIT_FAILURE;
		fprintf(stderr, "%s: %s, while parsing c%d.lua\n", getLogLevelString(static_cast<OCG_LogTypes>(type)), string, loading_card);
	};
	if(OCG_CreateDuel(&pduel, opts) != OCG_DUEL_CREATION_SUCCESS) {
		OCG_DestroyDuel(pduel);
		Error("Failed to create duel instance!\n");
	}
	if(!loadScript(constant->second))
		Error("Failed to load constant.lua\n");
	if(!loadScript(utility->second))
		Error("Failed to load utility.lua\n");

	OCG_NewCardInfo card{};
	card.team = card.duelist = card.con = 0;
	card.seq = 1;
	card.loc = LOCATION_DECK;
	card.pos = POS_FACEDOWN;
	for(const auto& [code, path] : scripts) {
		card.code = loading_card = code;
		OCG_DuelNewCard(pduel, card);
	}

	OCG_DestroyDuel(pduel);
	return status_code;
}
