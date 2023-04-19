#!/bin/make
.SILENT:

sinclude makefile.user

CXX := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --target=aarch64-linux-android26
CXXFLAGS := -std=c++20 -fPIC -fvisibility=hidden -Wall -Wno-dollar-in-identifier-extension -Wno-zero-length-array -Wno-gnu-statement-expression -Wno-format-pedantic -Wno-vla-extension -Wno-unused-function -Werror -pedantic-errors -Iextern/includes/libil2cpp/il2cpp/libil2cpp -Iextern/includes/codegen/include -Iextern/includes
LDFLAGS = -static-libstdc++ -shared -Wl,--no-undefined,--gc-sections,--fatal-warnings -Lextern/libs -l:$(notdir $(wildcard extern/libs/libbeatsaber-hook*.so)) -lcodegen -lpinkcore -llog
ifdef NDK
OBJDIR := .obj/$(shell $(CXX) -dumpmachine)
else
OBJDIR := .obj/unknown
ndk:
	$(error Android NDK path not set)
endif
FILES := src/main.cpp extern/includes/beatsaber-hook/src/inline-hook/And64InlineHook.cpp
OBJS := $(FILES:%=$(OBJDIR)/%.o)

qmod: MappingExtensions.qmod

libmappingextensions.so: $(OBJS) | ndk
	@echo "[cxx $@]"
	$(CXX) $(LDFLAGS) $(OBJS) -o "$@"

$(OBJDIR)/%.cpp.o: %.cpp extern makefile | ndk
	@echo "[cxx $(notdir $@)]"
	@mkdir -p "$(@D)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@" -MMD -MP

.obj/mod.json: extern makefile
	@echo "[printf $(notdir $@)]"
	@mkdir -p "$(@D)"
	printf "{\n\
		\"\$$schema\": \"https://raw.githubusercontent.com/Lauriethefish/QuestPatcher.QMod/main/QuestPatcher.QMod/Resources/qmod.schema.json\",\n\
		\"_QPVersion\": \"0.1.1\",\n\
		\"name\": \"Mapping Extensions\",\n\
		\"id\": \"MappingExtensions\",\n\
		\"author\": \"StackDoubleFlow, rxzz0, rcelyte\",\n\
		\"version\": \"0.22.3\",\n\
		\"packageId\": \"com.beatgames.beatsaber\",\n\
		\"packageVersion\": \"1.28.0_4124311467\",\n\
		\"description\": \"This adds a host of new things you can do with your maps as a mapper, and allows you to play said maps as a player. An update of the port of the PC original mod by Kyle 1413. Previously maintained by zoller27osu.\",\n\
		\"coverImage\": \"cover.png\",\n\
		\"dependencies\": [\n\
			{\n\
				\"version\": \"^0.33.0\",\n\
				\"id\": \"codegen\",\n\
				\"downloadIfMissing\": \"https://github.com/sc2ad/BeatSaber-Quest-Codegen/releases/download/v0.33.0/Codegen.qmod\"\n\
			}, {\n\
				\"version\": \"1.8.9\",\n\
				\"id\": \"pinkcore\",\n\
				\"downloadIfMissing\": \"https://github.com/BSMGPink/PinkCore/releases/download/v1.8.9/PinkCore.qmod\"\n\
			}\n\
		],\n\
		\"modFiles\": [\"libmappingextensions.so\"],\n\
		\"libraryFiles\": [\"$(notdir $(wildcard extern/libs/libbeatsaber-hook*.so))\"]\n\
	}" > .obj/mod.json

MappingExtensions.qmod: libmappingextensions.so .obj/mod.json
	@echo "[zip $@]"
	zip -j "$@" cover.png extern/libs/libbeatsaber-hook*.so libmappingextensions.so .obj/mod.json

extern:
	@echo "[qpm restore]"
	qpm-rust restore

clean:
	@echo "[cleaning]"
	rm -rf .obj/ extern/ include/ MappingExtensions.qmod libmappingextensions.so
	qpm-rust clear || true

.PHONY: clean ndk qmod

sinclude $(FILES:%=$(OBJDIR)/%.d)
