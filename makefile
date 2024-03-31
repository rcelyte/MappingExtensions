#!/bin/make
.SILENT:

sinclude makefile.user

CXX := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --target=aarch64-linux-android26
CXXFLAGS := -O2 -std=c++20 -fPIC -ffunction-sections -fdata-sections -fvisibility=hidden -fdeclspec \
	-Weverything -Wno-c++98-compat -Wno-pre-c++20-compat-pedantic -Werror -pedantic-errors \
	-isystem extern/includes/libil2cpp/il2cpp/libil2cpp -isystem extern/includes/bs-cordl/include -isystem extern/includes/fmt/fmt/include \
	-isystem extern/includes -DUNITY_2021 -DHAS_CODEGEN -DFMT_HEADER_ONLY
LDFLAGS = -O2 -s -static-libstdc++ -shared -Wl,--no-undefined,--gc-sections,--fatal-warnings \
	-Lextern/libs -l:$(notdir $(wildcard extern/libs/libbeatsaber-hook*.so)) -lsongcore -lpaperlog
ifdef NDK
OBJDIR := .obj/$(shell $(CXX) -dumpmachine)
else
OBJDIR := .obj/unknown
ndk:
	$(error Android NDK path not set)
endif
FILES := src/main.cpp
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
		\"modloader\": \"Scotland2\",\n\
		\"name\": \"Mapping Extensions\",\n\
		\"id\": \"MappingExtensions\",\n\
		\"author\": \"StackDoubleFlow, rxzz0, & rcelyte\",\n\
		\"version\": \"0.23.0\",\n\
		\"packageId\": \"com.beatgames.beatsaber\",\n\
		\"packageVersion\": \"1.35.0_8016709773\",\n\
		\"description\": \"This adds a host of new things you can do with your maps as a mapper, and allows you to play said maps as a player. An update of the port of the PC original mod by Kyle 1413. Previously maintained by zoller27osu.\",\n\
		\"coverImage\": \"cover.png\",\n\
		\"dependencies\": [\n\
			{\n\
				\"version\": \"^3.6.1\",\n\
				\"id\": \"paper\",\n\
				\"downloadIfMissing\": \"https://github.com/Fernthedev/paperlog/releases/download/v3.6.3/paperlog.qmod\"\n\
			}, {\n\
				\"version\": \"^1.0.0\",\n\
				\"id\": \"songcore\",\n\
				\"downloadIfMissing\": \"https://github.com/raineio/Quest-SongCore/releases/download/v1.1.2/SongCore.qmod\"\n\
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
