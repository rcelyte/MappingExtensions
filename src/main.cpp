// https://github.com/rxzz0/MappingExtensions/blob/main/src/main.cpp
// Refactored and updated to 1.24.0+ by rcelyte

#include <scotland2/shared/loader.hpp>

#include <BeatmapDataLoaderVersion2_6_0AndEarlier/BeatmapDataLoaderVersion2_6_0AndEarlier.hpp>
#include <BeatmapDataLoaderVersion3/BeatmapDataLoaderVersion3.hpp>
#include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveDataVersion2_6_0AndEarlier.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveDataVersion3.hpp>
#include <BeatmapSaveDataVersion4/BeatmapSaveDataVersion4.hpp>
#include <GlobalNamespace/BeatmapData.hpp>
#include <GlobalNamespace/BeatmapObjectsInTimeRowProcessor.hpp>
#include <GlobalNamespace/BeatmapObjectSpawnMovementData.hpp>
#include <GlobalNamespace/ColorNoteVisuals.hpp>
#include <GlobalNamespace/EnvironmentKeywords.hpp>
#include <GlobalNamespace/GameplayCoreSceneSetupData.hpp>
#include <GlobalNamespace/NoteBasicCutInfoHelper.hpp>
#include <GlobalNamespace/NoteCutDirectionExtensions.hpp>
#include <GlobalNamespace/NoteData.hpp>
#include <GlobalNamespace/ObstacleController.hpp>
#include <GlobalNamespace/ObstacleData.hpp>
#include <GlobalNamespace/SliderData.hpp>
#include <GlobalNamespace/SliderMeshController.hpp>
#include <GlobalNamespace/StaticBeatmapObjectSpawnMovementData.hpp>
#include <GlobalNamespace/WaypointData.hpp>
#include <System/Collections/Generic/LinkedList_1.hpp>
#include <System/Diagnostics/Stopwatch.hpp>

#include <sombrero/shared/FastQuaternion.hpp>
#include <sombrero/shared/FastVector2.hpp>
#include <sombrero/shared/FastVector3.hpp>

#include <beatsaber-hook/shared/utils/hooking.hpp>
#include <songcore/shared/SongCore.hpp>

SafePtr(void) -> SafePtr<void>;

constexpr bool UseOrigHooks = true;

const Paper::ConstLoggerContext<18> logger = {"MappingExtensions"};

static const std::string_view requirementNames[] = {
	"Mapping Extensions",
	"Mapping Extensions-Precision Placement",
	"Mapping Extensions-Extra Note Angles",
	"Mapping Extensions-More Lanes",
};

static bool active = false;
MAKE_HOOK_MATCH(GameplayCoreSceneSetupData_LoadTransformedBeatmapDataAsync, &GlobalNamespace::GameplayCoreSceneSetupData::LoadTransformedBeatmapDataAsync,
		System::Threading::Tasks::Task*, GlobalNamespace::GameplayCoreSceneSetupData *const self) {
	active = [self]() -> bool {
		auto *const beatmapLevel = il2cpp_utils::try_cast<SongCore::SongLoader::CustomBeatmapLevel>(self->beatmapLevel).value_or(nullptr);
		if(beatmapLevel == nullptr) {
			logger.warn("level missing SongCore metadata");
			return false;
		}
		const std::optional<std::reference_wrapper<SongCore::CustomJSONData::CustomSaveDataInfo>> info = beatmapLevel->get_CustomSaveDataInfo();
		if(!info.has_value()) {
			logger.warn("get_CustomSaveDataInfo() failed");
			return false;
		}
		std::optional<std::reference_wrapper<const SongCore::CustomJSONData::CustomSaveDataInfo::BasicCustomDifficultyBeatmapDetails>> details =
			info->get().TryGetCharacteristicAndDifficulty(self->beatmapKey.beatmapCharacteristic->serializedName, self->beatmapKey.difficulty);
		if(!details.has_value()) {
			logger.warn("TryGetCharacteristicAndDifficulty() failed");
			return false;
		}
		for(const std::string &req : details->get().requirements)
			for(const std::string_view name : requirementNames)
				if(req == name)
					return true;
		return false;
	}();
	logger.info("Should activate: {}", active);
	return GameplayCoreSceneSetupData_LoadTransformedBeatmapDataAsync(self);
}

MAKE_HOOK_MATCH(GameplayCoreSceneSetupData_LoadTransformedBeatmapData, &GlobalNamespace::GameplayCoreSceneSetupData::LoadTransformedBeatmapData,
		void, GlobalNamespace::GameplayCoreSceneSetupData *const self) {
	active = false;
	logger.info("Should activate: sync load");
	GameplayCoreSceneSetupData_LoadTransformedBeatmapData(self);
}

[[clang::no_destroy]] thread_local static std::function<void(GlobalNamespace::BeatmapObjectData*)> restoreHook =
	[](GlobalNamespace::BeatmapObjectData*) {};
MAKE_HOOK_MATCH(BeatmapData_AddBeatmapObjectData, &GlobalNamespace::BeatmapData::AddBeatmapObjectData, void, GlobalNamespace::BeatmapData *const self, GlobalNamespace::BeatmapObjectData *const beatmapObjectData) {
	restoreHook(beatmapObjectData);
	BeatmapData_AddBeatmapObjectData(self, beatmapObjectData);
}

template<class TData> struct LayerCache {
	template<class> struct RestoreData;
	template<> struct RestoreData<GlobalNamespace::NoteData> {
		GlobalNamespace::NoteLineLayer noteLineLayer;
		GlobalNamespace::NoteCutDirection cutDirection;
		RestoreData(BeatmapSaveDataVersion2_6_0AndEarlier::NoteData *const from) : noteLineLayer(from->get_lineLayer().value__), cutDirection(from->get_cutDirection().value__) {}
		RestoreData(BeatmapSaveDataVersion3::ColorNoteData *const from) : noteLineLayer(from->get_layer()), cutDirection(from->get_cutDirection().value__) {}
		RestoreData(BeatmapSaveDataVersion3::BombNoteData *const from) : noteLineLayer(from->get_layer()), cutDirection(0) {}
		void apply(GlobalNamespace::NoteData *const data) {
			data->noteLineLayer = this->noteLineLayer;
			data->cutDirection = this->cutDirection;
		}
	};
	template<> struct RestoreData<GlobalNamespace::ObstacleData> {
		GlobalNamespace::NoteLineLayer lineLayer;
		int32_t height;
		RestoreData(BeatmapSaveDataVersion2_6_0AndEarlier::ObstacleData *const from) : lineLayer(0), height(0) {
			const int32_t type = from->get_type().value__;
			if(type < 1000 || type > 4005000)
				return;
			const bool preciseStart = (type >= 4001);
			this->lineLayer = preciseStart * ((type - 4001) % 1000) * 5000 / 750 + 1334;
			this->height = (preciseStart ? (type - 4001) / 1000 : type - 1000) * 5 + 1000;
		}
		RestoreData(BeatmapSaveDataVersion3::ObstacleData *const from) : lineLayer(from->get_layer()), height(0) {}
		void apply(GlobalNamespace::ObstacleData *const data) {
			if(this->lineLayer.value__ != 0)
				data->lineLayer = this->lineLayer;
			if(this->height != 0)
				data->height = this->height;
		}
	};
	template<> struct RestoreData<GlobalNamespace::SliderData> {
		GlobalNamespace::NoteLineLayer headLineLayer, tailLineLayer;
		RestoreData(BeatmapSaveDataVersion2_6_0AndEarlier::SliderData *const from) : headLineLayer(from->get_headLineLayer().value__), tailLineLayer(from->get_tailLineLayer().value__) {}
		RestoreData(BeatmapSaveDataVersion3::SliderData *const from) : headLineLayer(from->get_headLayer()), tailLineLayer(from->get_tailLayer()) {}
		RestoreData(BeatmapSaveDataVersion3::BurstSliderData *const from) : headLineLayer(from->get_headLayer()), tailLineLayer(from->get_tailLayer()) {}
		void apply(GlobalNamespace::SliderData *const data) {
			data->headLineLayer = this->headLineLayer;
			data->headBeforeJumpLineLayer = this->headLineLayer;
			data->tailLineLayer = this->tailLineLayer;
			data->tailBeforeJumpLineLayer = this->tailLineLayer;
		}
	};
	template<> struct RestoreData<GlobalNamespace::WaypointData> {
		GlobalNamespace::NoteLineLayer lineLayer;
		RestoreData(BeatmapSaveDataVersion2_6_0AndEarlier::WaypointData *const from) : lineLayer(from->get_lineLayer().value__) {}
		RestoreData(BeatmapSaveDataVersion3::WaypointData *const from) : lineLayer(from->get_layer()) {}
		void apply(GlobalNamespace::WaypointData *const data) {
			data->lineLayer = this->lineLayer;
		}
	};

	using TIdent = std::array<uint8_t, sizeof(TData) - sizeof(System::Object)>;
	static inline TIdent ident(const TData *const data) {
		TIdent out;
		const std::span view = std::span(reinterpret_cast<const uint8_t*>(data), sizeof(*data)).subspan(sizeof(*data) - out.size());
		std::copy(view.begin(), view.end(), out.data());
		return out;
	}

	std::vector<std::pair<TIdent, RestoreData<TData>>> cache;
	std::vector<bool> matched;
	size_t head = 0, failCount = 0;

	LayerCache(const LayerCache&) = delete;
	template<class TFrom, class TConverter> LayerCache(System::Collections::Generic::List_1<TFrom*> *const list, SafePtr<TConverter> converter) :
			matched(static_cast<uint32_t>(list->get_Count())) {
		il2cpp_utils::cast<GlobalNamespace::BpmTimeProcessor>(converter.ptr()->_bpmTimeProcessor)->Reset();
		converter.ptr()->_rotationTimeProcessor->Reset();
		this->cache.reserve(matched.size());
		for(int32_t i = 0; i < static_cast<int32_t>(matched.size()); ++i) {
			TFrom *const item = list->get_Item(i);
			if(const std::optional<TData*> data = il2cpp_utils::try_cast<TData>(converter->Convert(item)))
				cache.emplace_back(ident(*data), RestoreData<TData>(item));
		}
	}

	bool restore(TData *const data) {
		for(size_t i = head; i < cache.size(); ++i) {
			if(matched[i] || cache[i].first != ident(data))
				continue;
			matched[i] = true;
			if(i == head) {
				do {
					++head;
				} while(head < cache.size() && matched[head]);
			}
			cache[i].second.apply(data);
			return true;
		}
		++failCount;
		return false;
	}
};

MAKE_HOOK_MATCH(BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData,
		&BeatmapDataLoaderVersion2_6_0AndEarlier::BeatmapDataLoader::GetBeatmapDataFromSaveData, GlobalNamespace::BeatmapData*,
		BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData *const beatmapSaveData, BeatmapSaveDataVersion4::LightshowSaveData *const defaultLightshowSaveData,
		const GlobalNamespace::BeatmapDifficulty beatmapDifficulty, const float startBpm, const bool loadingForDesignatedEnvironment,
		GlobalNamespace::EnvironmentKeywords *const environmentKeywords, GlobalNamespace::IEnvironmentLightGroups *const environmentLightGroups,
		GlobalNamespace::PlayerSpecificSettings *const playerSpecificSettings, GlobalNamespace::IBeatmapLightEventConverter *const lightEventConverter) {
	if(!active)
		return BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData(beatmapSaveData, defaultLightshowSaveData,
			beatmapDifficulty, startBpm, loadingForDesignatedEnvironment, environmentKeywords, environmentLightGroups, playerSpecificSettings, lightEventConverter);
	SafePtr<GlobalNamespace::BpmTimeProcessor> bpmState =
		GlobalNamespace::BpmTimeProcessor::New_ctor(startBpm, beatmapSaveData->get_events()->i___System__Collections__Generic__IReadOnlyList_1_T_());
	SafePtr<GlobalNamespace::RotationTimeProcessor> rotationState = // TODO: override rotations array
		GlobalNamespace::RotationTimeProcessor::New_ctor(beatmapSaveData->get_events()->i___System__Collections__Generic__IReadOnlyList_1_T_());
	LayerCache<GlobalNamespace::NoteData> noteCache(beatmapSaveData->get_notes(),
		SafePtr(BeatmapDataLoaderVersion2_6_0AndEarlier::BeatmapDataLoader::ColorNoteConverter::New_ctor(bpmState.ptr()->i___GlobalNamespace__IBeatToTimeConverter(), rotationState.ptr())));
	LayerCache<GlobalNamespace::ObstacleData> obstacleCache(beatmapSaveData->get_obstacles(),
		SafePtr(BeatmapDataLoaderVersion2_6_0AndEarlier::BeatmapDataLoader::ObstacleConverter::New_ctor(bpmState.ptr()->i___GlobalNamespace__IBeatToTimeConverter(), rotationState.ptr())));
	LayerCache<GlobalNamespace::SliderData> sliderCache(beatmapSaveData->get_sliders(),
		SafePtr(BeatmapDataLoaderVersion2_6_0AndEarlier::BeatmapDataLoader::SliderConverter::New_ctor(bpmState.ptr()->i___GlobalNamespace__IBeatToTimeConverter(), rotationState.ptr())));
	LayerCache<GlobalNamespace::WaypointData> waypointCache(beatmapSaveData->get_waypoints(),
		SafePtr(BeatmapDataLoaderVersion2_6_0AndEarlier::BeatmapDataLoader::WaypointConverter::New_ctor(bpmState.ptr()->i___GlobalNamespace__IBeatToTimeConverter(), rotationState.ptr())));
	logger.info("Restoring {} notes, {} obstacles, {} sliders, and {} waypoints",
		noteCache.cache.size(), obstacleCache.cache.size(), sliderCache.cache.size(), waypointCache.cache.size());
	restoreHook = [&](GlobalNamespace::BeatmapObjectData *const object) {
		if(GlobalNamespace::NoteData *const data = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(object).value_or(nullptr))
			noteCache.restore(data);
		else if(GlobalNamespace::ObstacleData *const obstacleData = il2cpp_utils::try_cast<GlobalNamespace::ObstacleData>(object).value_or(nullptr))
			obstacleCache.restore(obstacleData);
		else if(GlobalNamespace::SliderData *const sliderData = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(object).value_or(nullptr))
			sliderCache.restore(sliderData);
		else if(GlobalNamespace::WaypointData *const waypointData = il2cpp_utils::try_cast<GlobalNamespace::WaypointData>(object).value_or(nullptr))
			waypointCache.restore(waypointData);
	};
	SafePtr<GlobalNamespace::BeatmapData> result = BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData(
		beatmapSaveData, defaultLightshowSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment,
		environmentKeywords, environmentLightGroups, playerSpecificSettings, lightEventConverter);
	restoreHook = [](GlobalNamespace::BeatmapObjectData*) {};
	if(noteCache.failCount || obstacleCache.failCount || sliderCache.failCount || waypointCache.failCount) {
		logger.error("Failed to restore {} notes, {} obstacles, {} sliders, and {} waypoints",
			noteCache.failCount, obstacleCache.failCount, sliderCache.failCount, waypointCache.failCount);
		return nullptr;
	}
	return result.ptr();
}

MAKE_HOOK_MATCH(BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData, &BeatmapDataLoaderVersion3::BeatmapDataLoader::GetBeatmapDataFromSaveData,
		GlobalNamespace::BeatmapData*, BeatmapSaveDataVersion3::BeatmapSaveData *const beatmapSaveData, BeatmapSaveDataVersion4::LightshowSaveData *const defaultLightshowSaveData,
		const GlobalNamespace::BeatmapDifficulty beatmapDifficulty, const float startBpm, const bool loadingForDesignatedEnvironment,
		GlobalNamespace::EnvironmentKeywords *const environmentKeywords, GlobalNamespace::IEnvironmentLightGroups *const environmentLightGroups,
		GlobalNamespace::PlayerSpecificSettings *const playerSpecificSettings, GlobalNamespace::IBeatmapLightEventConverter *const lightEventConverter,
		System::Diagnostics::Stopwatch *const stopwatch) {
	if(!active)
		return BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData(beatmapSaveData, defaultLightshowSaveData, beatmapDifficulty,
			startBpm, loadingForDesignatedEnvironment, environmentKeywords, environmentLightGroups, playerSpecificSettings, lightEventConverter, stopwatch);
	SafePtr<GlobalNamespace::BpmTimeProcessor> bpmState =
		GlobalNamespace::BpmTimeProcessor::New_ctor(startBpm, beatmapSaveData->bpmEvents->i___System__Collections__Generic__IReadOnlyList_1_T_());
	SafePtr<GlobalNamespace::RotationTimeProcessor> rotationState =
		GlobalNamespace::RotationTimeProcessor::New_ctor(beatmapSaveData->rotationEvents->i___System__Collections__Generic__IReadOnlyList_1_T_());
	LayerCache<GlobalNamespace::NoteData> bombCache(beatmapSaveData->bombNotes,
		SafePtr(BeatmapDataLoaderVersion3::BeatmapDataLoader::BombNoteConverter::New_ctor(bpmState.ptr(), rotationState.ptr())));
	LayerCache<GlobalNamespace::NoteData> noteCache(beatmapSaveData->colorNotes,
		SafePtr(BeatmapDataLoaderVersion3::BeatmapDataLoader::ColorNoteConverter::New_ctor(bpmState.ptr(), rotationState.ptr())));
	LayerCache<GlobalNamespace::ObstacleData> obstacleCache(beatmapSaveData->obstacles,
		SafePtr(BeatmapDataLoaderVersion3::BeatmapDataLoader::ObstacleConverter::New_ctor(bpmState.ptr(), rotationState.ptr())));
	LayerCache<GlobalNamespace::SliderData> burstCache(beatmapSaveData->burstSliders,
		SafePtr(BeatmapDataLoaderVersion3::BeatmapDataLoader::BurstSliderConverter::New_ctor(bpmState.ptr(), rotationState.ptr())));
	LayerCache<GlobalNamespace::SliderData> sliderCache(beatmapSaveData->sliders,
		SafePtr(BeatmapDataLoaderVersion3::BeatmapDataLoader::SliderConverter::New_ctor(bpmState.ptr(), rotationState.ptr())));
	LayerCache<GlobalNamespace::WaypointData> waypointCache(beatmapSaveData->waypoints,
		SafePtr(BeatmapDataLoaderVersion3::BeatmapDataLoader::WaypointConverter::New_ctor(bpmState.ptr(), rotationState.ptr())));
	logger.info("Restoring {} notes, {} bombs, {} obstacles, {} sliders, {} burst sliders, and {} waypoints",
		noteCache.cache.size(), bombCache.cache.size(), obstacleCache.cache.size(), sliderCache.cache.size(), burstCache.cache.size(), waypointCache.cache.size());
	restoreHook = [&](GlobalNamespace::BeatmapObjectData *const object) {
		if(GlobalNamespace::NoteData *const data = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(object).value_or(nullptr)) {
			if(data->gameplayType == GlobalNamespace::NoteData::GameplayType::Bomb)
				bombCache.restore(data);
			else
				noteCache.restore(data);
		} else if(GlobalNamespace::ObstacleData *const obstacleData = il2cpp_utils::try_cast<GlobalNamespace::ObstacleData>(object).value_or(nullptr)) {
			obstacleCache.restore(obstacleData);
		} else if(GlobalNamespace::SliderData *const sliderData = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(object).value_or(nullptr)) {
			if(sliderData->sliderType == GlobalNamespace::SliderData::Type::Burst)
				burstCache.restore(sliderData);
			else
				sliderCache.restore(sliderData);
		} else if(GlobalNamespace::WaypointData *const waypointData = il2cpp_utils::try_cast<GlobalNamespace::WaypointData>(object).value_or(nullptr)) {
			waypointCache.restore(waypointData);
		}
	};
	SafePtr<GlobalNamespace::BeatmapData> result = BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData(
		beatmapSaveData, defaultLightshowSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment,
		environmentKeywords, environmentLightGroups, playerSpecificSettings, lightEventConverter, stopwatch);
	restoreHook = [](GlobalNamespace::BeatmapObjectData*) {};
	if(noteCache.failCount || bombCache.failCount || obstacleCache.failCount || sliderCache.failCount || burstCache.failCount || waypointCache.failCount) {
		logger.error("Failed to restore {} notes, {} bombs, {} obstacles, {} sliders, {} burst sliders, and {} waypoints",
			noteCache.failCount, bombCache.failCount, obstacleCache.failCount, sliderCache.failCount, burstCache.failCount, waypointCache.failCount);
		return nullptr;
	}
	return result.ptr();
}

MAKE_HOOK_MATCH(BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice,
		&GlobalNamespace::BeatmapObjectsInTimeRowProcessor::HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, void,
		GlobalNamespace::BeatmapObjectsInTimeRowProcessor *const self,
		GlobalNamespace::BeatmapObjectsInTimeRowProcessor::TimeSliceContainer_1<GlobalNamespace::BeatmapDataItem*>* allObjectsTimeSlice, float nextTimeSliceTime) {
	std::unordered_map<int32_t, std::vector<GlobalNamespace::NoteData*>> columns = {};
	System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::BeatmapDataItem*> *const items = allObjectsTimeSlice->items;
	for(int32_t i = 0, itemCount = items->i___System__Collections__Generic__IReadOnlyCollection_1_T_()->Count; i < itemCount; ++i) {
		if(GlobalNamespace::NoteData *const note = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(items->get_Item(i)).value_or(nullptr)) {
			std::vector<GlobalNamespace::NoteData*> *const column = &columns.try_emplace(note->lineIndex).first->second;
			column->insert(std::find_if(column->begin(), column->end(), [lineLayer = note->noteLineLayer](GlobalNamespace::NoteData *const e) {
				return e->noteLineLayer > lineLayer;
			}), note);
			note->lineIndex = std::clamp(note->lineIndex, 0, 3);
		}
	}
	BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice(self, allObjectsTimeSlice, nextTimeSliceTime);
	for(const auto& [lineIndex, notes] : columns) {
		for(uint32_t i = 0; i < notes.size(); ++i) {
			notes[i]->SetBeforeJumpNoteLineLayer(static_cast<int32_t>(i));
			notes[i]->lineIndex = lineIndex;
		}
	}
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetNoteOffset, &GlobalNamespace::BeatmapObjectSpawnMovementData::GetNoteOffset, UnityEngine::Vector3, GlobalNamespace::BeatmapObjectSpawnMovementData *const self, int32_t noteLineIndex, const GlobalNamespace::NoteLineLayer noteLineLayer) {
	const UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetNoteOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	return Sombrero::FastVector3(self->_rightVec) * (static_cast<float>(-self->noteLinesCount + 1) * .5f +
		static_cast<float>(noteLineIndex) * (GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f)) +
		Sombrero::FastVector3(0, GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer), 0);
}

MAKE_HOOK_MATCH(StaticBeatmapObjectSpawnMovementData_Get2DNoteOffset, &GlobalNamespace::StaticBeatmapObjectSpawnMovementData::Get2DNoteOffset, UnityEngine::Vector2, int32_t noteLineIndex, int32_t noteLinesCount, GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector2 result = StaticBeatmapObjectSpawnMovementData_Get2DNoteOffset(noteLineIndex, noteLinesCount, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	return UnityEngine::Vector2(
		static_cast<float>(-noteLinesCount + 1) * .5f + static_cast<float>(noteLineIndex) *
			(GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f),
		GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer));
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetObstacleOffset, &GlobalNamespace::BeatmapObjectSpawnMovementData::GetObstacleOffset, UnityEngine::Vector3, GlobalNamespace::BeatmapObjectSpawnMovementData *const self, int32_t noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer) {
	const UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetObstacleOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	return Sombrero::FastVector3(self->_rightVec) * (static_cast<float>(-self->noteLinesCount + 1) * .5f +
		static_cast<float>(noteLineIndex) * (GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f)) +
		Sombrero::FastVector3(0, GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer) +
			GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kObstacleVerticalOffset, 0);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer, &GlobalNamespace::BeatmapObjectSpawnMovementData::HighestJumpPosYForLineLayer, float, GlobalNamespace::BeatmapObjectSpawnMovementData *const self, GlobalNamespace::NoteLineLayer lineLayer) {
	float result = BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer(self, lineLayer);
	if(!active)
		return result;
	const float delta = (self->_topLinesHighestJumpPosY - self->_upperLinesHighestJumpPosY);
	if(lineLayer.value__ >= 1000 || lineLayer.value__ <= -1000)
		return self->_upperLinesHighestJumpPosY - delta - delta + self->_jumpOffsetYProvider->get_jumpOffsetY() +
			static_cast<float>(lineLayer.value__) * (delta / 1000.f);
	if(static_cast<uint32_t>(lineLayer.value__) > 2u)
		return self->_upperLinesHighestJumpPosY - delta + self->_jumpOffsetYProvider->get_jumpOffsetY() + (static_cast<float>(lineLayer.value__) * delta);
	return result;
}

MAKE_HOOK_MATCH(ColorNoteVisuals_HandleNoteControllerDidInit, &GlobalNamespace::ColorNoteVisuals::HandleNoteControllerDidInit,
		void, GlobalNamespace::ColorNoteVisuals *const self, GlobalNamespace::NoteControllerBase *const noteController) {
	if(active) {
		GlobalNamespace::NoteData *const note = self->_noteController->noteData;
		const int32_t cutDirection = note->cutDirection.value__;
		if(cutDirection >= 2000 && cutDirection <= 2360) {
			note->cutDirection = GlobalNamespace::NoteCutDirection::Any;
			ColorNoteVisuals_HandleNoteControllerDidInit(self, noteController);
			note->cutDirection = cutDirection;
			return;
		}
	}
	ColorNoteVisuals_HandleNoteControllerDidInit(self, noteController);
}

MAKE_HOOK_MATCH(NoteBasicCutInfoHelper_GetBasicCutInfo, &GlobalNamespace::NoteBasicCutInfoHelper::GetBasicCutInfo, void, UnityEngine::Transform *const noteTransform,
		const GlobalNamespace::ColorType colorType, GlobalNamespace::NoteCutDirection cutDirection, const GlobalNamespace::SaberType saberType, const float saberBladeSpeed,
		const UnityEngine::Vector3 cutDirVec, const float cutAngleTolerance, const ByRef<bool> directionOK, const ByRef<bool> speedOK, const ByRef<bool> saberTypeOK,
		const ByRef<float> cutDirDeviation, const ByRef<float> cutDirAngle) {
	if(active && cutDirection.value__ >= 2000 && cutDirection.value__ <= 2360)
		cutDirection = GlobalNamespace::NoteCutDirection::Any;
	NoteBasicCutInfoHelper_GetBasicCutInfo(noteTransform, colorType, cutDirection, saberType, saberBladeSpeed, cutDirVec, cutAngleTolerance, directionOK, speedOK, saberTypeOK, cutDirDeviation, cutDirAngle);
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Rotation, &GlobalNamespace::NoteCutDirectionExtensions::Rotation, UnityEngine::Quaternion, GlobalNamespace::NoteCutDirection cutDirection, float offset) {
	UnityEngine::Quaternion result = NoteCutDirectionExtensions_Rotation(cutDirection, offset);
	if(!active)
		return result;
	if(cutDirection.value__ >= 1000 && cutDirection.value__ <= 1360) {
		result = UnityEngine::Quaternion();
		result.set_eulerAngles(UnityEngine::Vector3(0, 0, static_cast<float>(1000 - cutDirection.value__)));
	} else if(cutDirection.value__ >= 2000 && cutDirection.value__ <= 2360) {
		result = UnityEngine::Quaternion();
		result.set_eulerAngles(UnityEngine::Vector3(0, 0, static_cast<float>(2000 - cutDirection.value__)));
	}
	return result;
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Direction,
		&GlobalNamespace::NoteCutDirectionExtensions::Direction, UnityEngine::Vector2, const GlobalNamespace::NoteCutDirection cutDirection) {
	UnityEngine::Vector2 result = {};
	if constexpr(UseOrigHooks) {
		switch(cutDirection) {
			case GlobalNamespace::NoteCutDirection::Left: result = UnityEngine::Vector2(-1, 0); break;
			case GlobalNamespace::NoteCutDirection::Right: result = UnityEngine::Vector2(1, 0); break;
			case GlobalNamespace::NoteCutDirection::Up: result = UnityEngine::Vector2(0, 1); break;
			case GlobalNamespace::NoteCutDirection::Down: result = UnityEngine::Vector2(0, -1); break;
			case GlobalNamespace::NoteCutDirection::UpLeft: result = UnityEngine::Vector2(-.7071f, .7071f); break;
			case GlobalNamespace::NoteCutDirection::UpRight: result = UnityEngine::Vector2(.7071f, .7071f); break;
			case GlobalNamespace::NoteCutDirection::DownLeft: result = UnityEngine::Vector2(-.7071f, -.7071f); break;
			case GlobalNamespace::NoteCutDirection::DownRight: result = UnityEngine::Vector2(.7071f, -.7071f); break;
			default:;
		}
	} else {
		result = NoteCutDirectionExtensions_Direction(cutDirection);
	}
	if(!active)
		return result;
	int32_t offset = 2000;
	if(cutDirection.value__ >= 1000 && cutDirection.value__ <= 1360)
		offset = 1000;
	else if(cutDirection.value__ < 2000 || cutDirection.value__ > 2360)
		return result;
	Sombrero::FastQuaternion quaternion = Sombrero::FastQuaternion();
	quaternion.set_eulerAngles(UnityEngine::Vector3(0, 0, static_cast<float>(offset - cutDirection.value__)));
	Sombrero::FastVector3 dir = quaternion * Sombrero::FastVector3::down();
	return UnityEngine::Vector2(dir.x, dir.y);
}

MAKE_HOOK_MATCH_NO_CATCH(NoteCutDirectionExtensions_RotationAngle,
		&GlobalNamespace::NoteCutDirectionExtensions::RotationAngle, float, const GlobalNamespace::NoteCutDirection cutDirection) {
	float result = 0;
	if constexpr(UseOrigHooks) {
		switch(cutDirection) {
			case GlobalNamespace::NoteCutDirection::Left: result = -90; break;
			case GlobalNamespace::NoteCutDirection::Right: result = 90; break;
			case GlobalNamespace::NoteCutDirection::Up: result = -180; break;
			case GlobalNamespace::NoteCutDirection::Down: result = 0; break;
			case GlobalNamespace::NoteCutDirection::UpLeft: result = -135; break;
			case GlobalNamespace::NoteCutDirection::UpRight: result = 135; break;
			case GlobalNamespace::NoteCutDirection::DownLeft: result = -45; break;
			case GlobalNamespace::NoteCutDirection::DownRight: result = 45; break;
			default:;
		}
	} else {
		result = NoteCutDirectionExtensions_RotationAngle(cutDirection);
	}
	if(!active)
		return result;
	if(cutDirection.value__ >= 1000 && cutDirection.value__ <= 1360)
		return static_cast<float>(1000 - cutDirection.value__);
	if(cutDirection.value__ >= 2000 && cutDirection.value__ <= 2360)
		return static_cast<float>(2000 - cutDirection.value__);
	return result;
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Mirrored, &GlobalNamespace::NoteCutDirectionExtensions::Mirrored, GlobalNamespace::NoteCutDirection, GlobalNamespace::NoteCutDirection cutDirection) {
	GlobalNamespace::NoteCutDirection result = NoteCutDirectionExtensions_Mirrored(cutDirection);
	if(!active)
		return result;
	if(cutDirection.value__ >= 1000 && cutDirection.value__ <= 1360)
		return 2360 - cutDirection.value__;
	if(cutDirection.value__ >= 2000 && cutDirection.value__ <= 2360)
		return 4360 - cutDirection.value__;
	return result;
}

static std::optional<int32_t> MirrorPrecisionLineIndex(const int32_t lineIndex) {
	if(lineIndex >= 1000 || lineIndex <= -1000)
		return ((lineIndex > -1000 && lineIndex < 4000) ? 5000 : 3000) - lineIndex;
	if(static_cast<uint32_t>(lineIndex) > 3u)
		return 3 - lineIndex;
	return std::nullopt;
}

MAKE_HOOK_MATCH(NoteData_Mirror, &GlobalNamespace::NoteData::Mirror, void, GlobalNamespace::NoteData *const self, int32_t lineCount) {
	const int32_t lineIndex = self->lineIndex, flipLineIndex = self->flipLineIndex;
	NoteData_Mirror(self, lineCount);
	if(!active)
		return;
	if(const std::optional<int32_t> newLineIndex = MirrorPrecisionLineIndex(lineIndex))
		self->set_lineIndex(*newLineIndex);
	if(const std::optional<int32_t> newFlipLineIndex = MirrorPrecisionLineIndex(flipLineIndex))
		self->set_flipLineIndex(*newFlipLineIndex);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetObstacleSpawnData, &GlobalNamespace::BeatmapObjectSpawnMovementData::GetObstacleSpawnData,
		GlobalNamespace::ObstacleSpawnData, GlobalNamespace::BeatmapObjectSpawnMovementData *const self, GlobalNamespace::ObstacleData *const obstacleData) {
	GlobalNamespace::ObstacleSpawnData result = BeatmapObjectSpawnMovementData_GetObstacleSpawnData(self, obstacleData);
	float obstacleWidth = static_cast<float>(obstacleData->width);
	if(!active)
		return result;
	if(obstacleWidth <= -1000 || obstacleWidth >= 1000) {
		result.moveOffset.x = result.moveOffset.x - (obstacleWidth * .6f - .6f) * .5f;
		if(obstacleWidth <= -1000)
			obstacleWidth += 2000;
		obstacleWidth = (obstacleWidth - 1000) / 1000 * .6f;
		result.moveOffset.x = result.moveOffset.x + (obstacleWidth - .6f) * .5f;
		result.obstacleWidth = obstacleWidth;
	}

	const float height = static_cast<float>(obstacleData->height), layerHeight = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::get_layerHeight();
	if(height <= -1000)
		result.obstacleHeight = (height + 2000) / 1000 * layerHeight;
	else if(height >= 1000)
		result.obstacleHeight = (height - 1000) / 1000 * layerHeight;
	else if(height > 2)
		result.obstacleHeight = height * layerHeight;
	return result;
}

static int32_t ToNormalizedPrecisionIndex(int32_t index) {
	if(index <= -1000)
		return index + 1000;
	if(index >= 1000)
		return index - 1000;
	return index * 1000;
}

MAKE_HOOK_MATCH(ObstacleData_Mirror, &GlobalNamespace::ObstacleData::Mirror, void, GlobalNamespace::ObstacleData *const self, int32_t lineCount) {
	int32_t lineIndex = self->lineIndex;
	ObstacleData_Mirror(self, lineCount);
	if(!active)
		return;
	if(lineIndex >= 1000 || lineIndex <= -1000 || self->width >= 1000 || self->width <= -1000) {
		const int32_t newIndex = 4000 - ToNormalizedPrecisionIndex(lineIndex) - ToNormalizedPrecisionIndex(self->width);
		self->lineIndex = (newIndex < 0) ? newIndex - 1000 : newIndex + 1000;
	} else if(static_cast<uint32_t>(lineIndex) > 3u) {
		self->lineIndex = 4 - lineIndex - self->width;
	}
}

MAKE_HOOK_MATCH(SliderData_Mirror, &GlobalNamespace::SliderData::Mirror, void, GlobalNamespace::SliderData *const self, int32_t lineCount) {
	const int32_t headLineIndex = self->headLineIndex, tailLineIndex = self->tailLineIndex;
	SliderData_Mirror(self, lineCount);
	if(!active)
		return;
	if(const std::optional<int32_t> newHeadLineIndex = MirrorPrecisionLineIndex(headLineIndex))
		self->headLineIndex = *newHeadLineIndex;
	if(const std::optional<int32_t> newTailLineIndex = MirrorPrecisionLineIndex(tailLineIndex))
		self->tailLineIndex = *newTailLineIndex;
}

MAKE_HOOK_MATCH(SliderMeshController_CutDirectionToControlPointPosition, &GlobalNamespace::SliderMeshController::CutDirectionToControlPointPosition, UnityEngine::Vector3, GlobalNamespace::NoteCutDirection noteCutDirection) {
	UnityEngine::Vector3 result = {};
	if constexpr(UseOrigHooks) {
		switch(noteCutDirection) {
			case GlobalNamespace::NoteCutDirection::Up: result = UnityEngine::Vector3(0, 1, -1e-05f); break;
			case GlobalNamespace::NoteCutDirection::Down: result = UnityEngine::Vector3(0, -1, -1e-05f); break;
			case GlobalNamespace::NoteCutDirection::Left: result = UnityEngine::Vector3(-1, 0, -1e-05f); break;
			case GlobalNamespace::NoteCutDirection::Right: result = UnityEngine::Vector3(1, 0, -1e-05f); break;
			case GlobalNamespace::NoteCutDirection::UpLeft: result = UnityEngine::Vector3(-.70710677f, .70710677f, -1e-05f); break;
			case GlobalNamespace::NoteCutDirection::UpRight: result = UnityEngine::Vector3(.70710677f, .70710677f, -1e-05f); break;
			case GlobalNamespace::NoteCutDirection::DownLeft: result = UnityEngine::Vector3(-.70710677f, -.70710677f, -1e-05f); break;
			case GlobalNamespace::NoteCutDirection::DownRight: result = UnityEngine::Vector3(.70710677f, -.70710677f, -1e-05f); break;
			default:;
		}
	} else {
		result = SliderMeshController_CutDirectionToControlPointPosition(noteCutDirection);
	}
	if(!active)
		return result;
	if(noteCutDirection.value__ >= 1000 && noteCutDirection.value__ <= 1360) {
		Sombrero::FastQuaternion quaternion = Sombrero::FastQuaternion();
		quaternion.set_eulerAngles(UnityEngine::Vector3(0, 0, static_cast<float>(1000 - noteCutDirection.value__)));
		return quaternion * Sombrero::FastVector3::down();
	}
	if(noteCutDirection.value__ >= 2000 && noteCutDirection.value__ <= 2360) {
		Sombrero::FastQuaternion quaternion = Sombrero::FastQuaternion();
		quaternion.set_eulerAngles(UnityEngine::Vector3(0, 0, static_cast<float>(2000 - noteCutDirection.value__)));
		return quaternion * Sombrero::FastVector3::down();
	}
	return result;
}

MAKE_HOOK_MATCH_NO_CATCH(StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer,
		&GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer, float, const GlobalNamespace::NoteLineLayer lineLayer) {
	float result = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kTopLinesYPos;
	if constexpr(UseOrigHooks) {
		switch(lineLayer) {
			case GlobalNamespace::NoteLineLayer::Base: result = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kBaseLinesYPos; break;
			case GlobalNamespace::NoteLineLayer::Upper: result = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos; break;
			default:;
		}
	} else {
		result = StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer(lineLayer);
	}
	if(!active)
		return result;
	constexpr float delta = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kTopLinesYPos - GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos;
	if(lineLayer.value__ >= 1000 || lineLayer.value__ <= -1000)
		return GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta * 2 + static_cast<float>(lineLayer.value__) * (delta / 1000.f);
	if(static_cast<uint32_t>(lineLayer.value__) > 2u)
		return GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta + static_cast<float>(lineLayer.value__) * delta;
	return result;
}

extern "C" void setup(CModInfo*);
extern "C" [[gnu::visibility("default")]] void setup(CModInfo *const modInfo) {
	*modInfo = {
		.id = "MappingExtensions",
		.version = "0.25.3",
		.version_long = 18,
	};
	logger.info("Leaving setup!");
}

extern "C" void late_load();
extern "C" [[gnu::visibility("default")]] void late_load() {
	il2cpp_functions::Init();

	Hooking::InstallHook<Hook_GameplayCoreSceneSetupData_LoadTransformedBeatmapDataAsync>(logger);
	Hooking::InstallHook<Hook_GameplayCoreSceneSetupData_LoadTransformedBeatmapData>(logger);
	if(const std::vector<modloader::ModData> loaded = modloader::get_loaded(); std::find_if(loaded.begin(), loaded.end(), [](const modloader::ModData &data) {
				return data.info.id == "CustomJSONData";
			}) == loaded.end()) {
		Hooking::InstallHook<Hook_BeatmapData_AddBeatmapObjectData>(logger);
		Hooking::InstallHook<Hook_BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData>(logger);
		Hooking::InstallHook<Hook_BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData>(logger);
		// Hooking::InstallHook<Hook_BeatmapDataLoaderVersion4_BeatmapDataLoader_GetBeatmapDataFromSaveData>(logger); // TODO: implement
	}

	Hooking::InstallHook<Hook_BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice>(logger);
	Hooking::InstallHook<Hook_BeatmapObjectSpawnMovementData_GetNoteOffset>(logger);
	Hooking::InstallHook<Hook_StaticBeatmapObjectSpawnMovementData_Get2DNoteOffset>(logger);
	Hooking::InstallHook<Hook_BeatmapObjectSpawnMovementData_GetObstacleOffset>(logger);
	Hooking::InstallHook<Hook_BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer>(logger);
	Hooking::InstallHook<Hook_ColorNoteVisuals_HandleNoteControllerDidInit>(logger);
	Hooking::InstallHook<Hook_NoteBasicCutInfoHelper_GetBasicCutInfo>(logger);
	Hooking::InstallHook<Hook_NoteCutDirectionExtensions_Rotation>(logger);
	Hooking::InstallHook<Hook_NoteCutDirectionExtensions_Mirrored>(logger);
	Hooking::InstallHook<Hook_NoteData_Mirror>(logger);
	Hooking::InstallHook<Hook_BeatmapObjectSpawnMovementData_GetObstacleSpawnData>(logger);
	Hooking::InstallHook<Hook_ObstacleData_Mirror>(logger);
	Hooking::InstallHook<Hook_SliderData_Mirror>(logger);
	if constexpr(UseOrigHooks) {
		Hooking::InstallOrigHook<Hook_NoteCutDirectionExtensions_Direction>(logger);
		Hooking::InstallOrigHook<Hook_NoteCutDirectionExtensions_RotationAngle>(logger);
		Hooking::InstallOrigHook<Hook_SliderMeshController_CutDirectionToControlPointPosition>(logger);
		Hooking::InstallOrigHook<Hook_StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer>(logger);
	} else {
		Hooking::InstallHook<Hook_NoteCutDirectionExtensions_Direction>(logger);
		Hooking::InstallHook<Hook_NoteCutDirectionExtensions_RotationAngle>(logger);
		Hooking::InstallHook<Hook_SliderMeshController_CutDirectionToControlPointPosition>(logger);
		Hooking::InstallHook<Hook_StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer>(logger);
	}

	logger.info("Installed ME hooks successfully!");
	for(const std::string_view name : requirementNames)
		SongCore::API::Capabilities::RegisterCapability(name);
}

#include <GlobalNamespace/zzzz__BeatmapData_impl.hpp>
#include <GlobalNamespace/zzzz__BpmTimeProcessor_impl.hpp>
#include <GlobalNamespace/zzzz__IJumpOffsetYProvider_impl.hpp>
#include <GlobalNamespace/zzzz__NoteControllerBase_impl.hpp>
#include <GlobalNamespace/zzzz__RotationTimeProcessor_impl.hpp>
#include <System/Collections/Generic/zzzz__IReadOnlyCollection_1_impl.hpp>
#include <System/Collections/Generic/zzzz__IReadOnlyList_1_impl.hpp>
#include <System/Collections/Generic/zzzz__LinkedListNode_1_impl.hpp>
