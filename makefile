#!/bin/make
MAKEFLAGS += -j
.SILENT:

sinclude makefile.user

CXX := $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --target=aarch64-linux-android26
CXXFLAGS := -std=c++20 -fPIC -ffunction-sections -fdata-sections -fvisibility=hidden -fdeclspec \
	-Weverything -Wno-c++98-compat -Wno-pre-c++20-compat-pedantic -Wno-switch-enum -Werror -pedantic-errors \
	-isystem extern/includes/libil2cpp/il2cpp/libil2cpp -isystem extern/includes/bs-cordl/include -isystem extern/includes/fmt/fmt/include \
	-isystem extern/includes -isystem extern/includes/libil2cpp/il2cpp/external/baselib/Include \
	-isystem extern/includes/libil2cpp/il2cpp/external/baselib/Platforms/Android/Include -DUNITY_2021 -DHAS_CODEGEN -DFMT_HEADER_ONLY
LDFLAGS = -static-libstdc++ -shared -Wl,--no-undefined,--gc-sections,--fatal-warnings \
	-Lextern/libs -l:$(notdir $(wildcard extern/libs/libbeatsaber-hook*.so)) -lsongcore -lpaper2_scotland2 -lsl2 -llog
ifdef BUILD_DEBUG
CXXFLAGS += -Og
LDFLAGS += -Og -g
else
CXXFLAGS += -O2
LDFLAGS += -O2 -s
endif
ifdef NDK
OBJDIR := .obj/$(shell $(CXX) -dumpmachine)
else
OBJDIR := .obj/unknown
ndk:
	$(error Android NDK path not set)
endif
FILES := src/main.cpp .obj/And64InlineHook.cpp
OBJS := $(FILES:%=$(OBJDIR)/%.o)

qmod: MappingExtensions.qmod

libmappingextensions.so: $(OBJS) | ndk
	@echo "[cxx $@]"
	$(CXX) $(LDFLAGS) $(OBJS) -o "$@"

$(OBJDIR)/.obj/%.cpp.o: CXXFLAGS += -w -isystem extern/includes/beatsaber-hook/shared/inline-hook
$(OBJDIR)/%.cpp.o: %.cpp extern makefile | ndk
	@echo "[cxx $(notdir $@)]"
	@mkdir -p "$(@D)"
	$(CXX) $(CXXFLAGS) -c "$<" -o "$@" -MMD -MP

extern/includes/bs-cordl/include/version.txt extern/includes/beatsaber-hook/shared/inline-hook/And64InlineHook.cpp: extern
.obj/And64InlineHook.cpp: extern/includes/beatsaber-hook/shared/inline-hook/And64InlineHook.cpp
	@echo "[sed $(notdir $@)]"
	@mkdir -p "$(@D)"
	sed 's/__attribute__((visibility("default")))//' $< > $@

.obj/mod.json: extern/includes/bs-cordl/include/version.txt makefile
	@echo "[printf $(notdir $@)]"
	@mkdir -p "$(@D)"
	printf "{\n\
		\"\$$schema\": \"https://raw.githubusercontent.com/Lauriethefish/QuestPatcher.QMod/main/QuestPatcher.QMod/Resources/qmod.schema.json\",\n\
		\"_QPVersion\": \"1.2.0\",\n\
		\"modloader\": \"Scotland2\",\n\
		\"name\": \"Mapping Extensions\",\n\
		\"id\": \"MappingExtensions\",\n\
		\"author\": \"StackDoubleFlow, rxzz0, & rcelyte\",\n\
		\"version\": \"0.25.0\",\n\
		\"packageId\": \"com.beatgames.beatsaber\",\n\
		\"packageVersion\": \"%s\",\n\
		\"description\": \"This adds a host of new things you can do with your maps as a mapper, and allows you to play said maps as a player. An update of the port of the PC original mod by Kyle 1413. Previously maintained by zoller27osu.\",\n\
		\"coverImage\": \"cover.jpg\",\n\
		\"dependencies\": [\n\
			{\n\
				\"version\": \"^6.4.2\",\n\
				\"id\": \"beatsaber-hook\",\n\
				\"downloadIfMissing\": \"https://github.com/QuestPackageManager/beatsaber-hook/releases/download/v6.4.2/beatsaber-hook.qmod\"\n\
			}, {\n\
				\"version\": \"^4.6.4\",\n\
				\"id\": \"paper2_scotland2\",\n\
				\"downloadIfMissing\": \"https://github.com/Fernthedev/paperlog/releases/download/v4.6.4/paper2_scotland2.qmod\"\n\
			}, {\n\
				\"version\": \"^1.1.20\",\n\
				\"id\": \"songcore\",\n\
				\"downloadIfMissing\": \"https://github.com/raineio/Quest-SongCore/releases/download/v1.1.20/SongCore.qmod\"\n\
			}, {\n\
				\"version\": \"^0.23.0\",\n\
				\"id\": \"custom-json-data\",\n\
				\"downloadIfMissing\": \"https://github.com/StackDoubleFlow/CustomJSONData/releases/download/v0.23.0/custom-json-data.qmod\",\n\
				\"required\": false\n\
			}, {\n\
				\"version\": \"^1.5.4\",\n\
				\"id\": \"NoodleExtensions\",\n\
				\"downloadIfMissing\": \"https://github.com/StackDoubleFlow/NoodleExtensions/releases/download/v1.5.4/NoodleExtensions.qmod\",\n\
				\"required\": false\n\
			}\n\
		],\n\
		\"lateModFiles\": [\"libmappingextensions.so\"]\n\
	}" "$$(cat $<)" > .obj/mod.json

MappingExtensions.qmod: cover.jpg libmappingextensions.so .obj/mod.json
	@echo "[zip $@]"
	zip -j "$@" $^

extern: qpm.json
	@echo "[qpm restore]"
	qpm-rust restore

clean:
	@echo "[cleaning]"
	rm -rf .obj/ extern/ include/ MappingExtensions.qmod libmappingextensions.so
	qpm-rust clear || true

.PHONY: clean ndk qmod

sinclude $(FILES:%=$(OBJDIR)/%.d)
