// https://github.com/rxzz0/MappingExtensions/blob/main/src/main.cpp
// Refactored and updated to 1.24.0+ by rcelyte

#include <scotland2/shared/loader.hpp>

#include <BeatmapDataLoaderVersion2_6_0AndEarlier/BeatmapDataLoaderVersion2_6_0AndEarlier.hpp>
#include <BeatmapDataLoaderVersion3/BeatmapDataLoaderVersion3.hpp>
#include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveDataVersion2_6_0AndEarlier.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveDataVersion3.hpp>
#include <BeatmapSaveDataVersion4/BeatmapSaveDataVersion4.hpp>
#include <GlobalNamespace/BeatmapObjectsInTimeRowProcessor.hpp>
#include <GlobalNamespace/BeatmapObjectSpawnMovementData.hpp>
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
#include <System/Diagnostics/Stopwatch.hpp>

#include <sombrero/shared/FastQuaternion.hpp>
#include <sombrero/shared/FastVector2.hpp>
#include <sombrero/shared/FastVector3.hpp>

#include <beatsaber-hook/shared/utils/hooking.hpp>
#include <songcore/shared/SongCore.hpp>

const Paper::ConstLoggerContext<18> logger = {"MappingExtensions"};

static const std::string_view requirementNames[] = {
	"Mapping Extensions",
	"Mapping Extensions-Precision Placement",
	"Mapping Extensions-Extra Note Angles",
	"Mapping Extensions-More Lanes",
};

static bool ShouldActivate(GlobalNamespace::GameplayCoreSceneSetupData *const data) {
	auto *const beatmapLevel = il2cpp_utils::try_cast<SongCore::SongLoader::CustomBeatmapLevel>(data->beatmapLevel).value_or(nullptr);
	if(beatmapLevel == nullptr) {
		logger.warn("level missing SongCore metadata");
		return false;
	}
	SongCore::CustomJSONData::CustomLevelInfoSaveData::BasicCustomDifficultyBeatmapDetails details = {};
	if(!beatmapLevel->standardLevelInfoSaveData->TryGetCharacteristicAndDifficulty(
			data->beatmapKey.beatmapCharacteristic->serializedName, data->beatmapKey.difficulty, *&details)) {
		logger.warn("TryGetCharacteristicAndDifficulty() failed");
		return false;
	}
	for(const std::string &req : details.requirements)
		for(const std::string_view name : requirementNames)
			if(req == name)
				return true;
	return false;
}

static bool active = false;
MAKE_HOOK_MATCH(GameplayCoreSceneSetupData_LoadTransformedBeatmapDataAsync, &GlobalNamespace::GameplayCoreSceneSetupData::LoadTransformedBeatmapDataAsync, System::Threading::Tasks::Task*, GlobalNamespace::GameplayCoreSceneSetupData *const self) {
	active = ShouldActivate(self);
	logger.info("ShouldActivate(): {}", active);
	return GameplayCoreSceneSetupData_LoadTransformedBeatmapDataAsync(self);
}

MAKE_HOOK_MATCH(GameplayCoreSceneSetupData_LoadTransformedBeatmapData, &GlobalNamespace::GameplayCoreSceneSetupData::LoadTransformedBeatmapData, void, GlobalNamespace::GameplayCoreSceneSetupData *const self) {
	active = false;
	logger.info("ShouldActivate(): sync");
	GameplayCoreSceneSetupData_LoadTransformedBeatmapData(self);
}

/* PC version hooks */

static inline float SpawnRotationForEventValue(float orig, int32_t index) {
	if(index >= 1000 && index <= 1720)
		return static_cast<float>(index - 1360);
	return orig;
}

static inline int32_t GetHeightForObstacleType(const int32_t orig, const int32_t obstacleType) {
	if((obstacleType < 1000 || obstacleType > 4000) && (obstacleType < 4001 || obstacleType > 4005000))
		return orig;
	return ((obstacleType >= 4001 && obstacleType <= 4100000) ? (obstacleType - 4001) / 1000 : obstacleType - 1000) * 5 + 1000;
}

static inline GlobalNamespace::NoteLineLayer GetLayerForObstacleType(const GlobalNamespace::NoteLineLayer orig, const int32_t obstacleType) {
	if((obstacleType < 1000 || obstacleType > 4000) && (obstacleType < 4001 || obstacleType > 4005000))
		return orig;
	const int32_t startHeight = (obstacleType >= 4001 && obstacleType <= 4100000) ? (obstacleType - 4001) % 1000 : 0;
	return static_cast<int32_t>(static_cast<float>(startHeight) * (20.f / 3)) + 1334;
}

#include "conversions.hpp"
MAKE_HOOK_MATCH(BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData, &BeatmapDataLoaderVersion2_6_0AndEarlier::BeatmapDataLoader::GetBeatmapDataFromSaveData, GlobalNamespace::BeatmapData*, BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData *const beatmapSaveData, BeatmapSaveDataVersion4::LightshowSaveData *const defaultLightshowSaveData, const GlobalNamespace::BeatmapDifficulty beatmapDifficulty, const float startBpm, const bool loadingForDesignatedEnvironment, GlobalNamespace::EnvironmentKeywords *const environmentKeywords, GlobalNamespace::IEnvironmentLightGroups *const environmentLightGroups, GlobalNamespace::PlayerSpecificSettings *const playerSpecificSettings) {
	if(!active)
		return BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData(beatmapSaveData, defaultLightshowSaveData,
			beatmapDifficulty, startBpm, loadingForDesignatedEnvironment, environmentKeywords, environmentLightGroups, playerSpecificSettings);
	System::Collections::Generic::List_1<BeatmapSaveDataVersion2_6_0AndEarlier::EventData*> *const events = beatmapSaveData->get_events();
	const std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> bpmChanges = ConvertBpmChanges(startBpm, events);
	LayerCache noteCache(beatmapSaveData->get_notes(), &bpmChanges);
	LayerCache obstacleCache(beatmapSaveData->get_obstacles(), &bpmChanges);
	LayerCache sliderCache(beatmapSaveData->get_sliders(), &bpmChanges);
	LayerCache waypointCache(beatmapSaveData->get_waypoints(), &bpmChanges);
	LayerCache rotationCache(events, &bpmChanges, +[](BeatmapSaveDataVersion2_6_0AndEarlier::EventData *const event) {
		const BeatmapSaveDataCommon::BeatmapEventType type = event->get_type();
		return type == BeatmapSaveDataCommon::BeatmapEventType::Event14 || type == BeatmapSaveDataCommon::BeatmapEventType::Event15;
	});
	logger.info("Restoring {} notes, {} obstacles, {} sliders, {} waypoints, and {} rotation events",
		noteCache.cache.size(), obstacleCache.cache.size(), sliderCache.cache.size(), waypointCache.cache.size(), rotationCache.cache.size());
	SafePtr<GlobalNamespace::BeatmapData> result = BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData(
		beatmapSaveData, defaultLightshowSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment,
		environmentKeywords, environmentLightGroups, playerSpecificSettings);
	for(System::Collections::Generic::LinkedListNode_1<GlobalNamespace::BeatmapDataItem*> *iter = result->get_allBeatmapDataItems()->head, *const end = iter ? iter->prev : nullptr; iter; iter = iter->next) {
		GlobalNamespace::BeatmapDataItem *const item = iter->item;
		if(GlobalNamespace::NoteData *const data = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(item).value_or(nullptr)) {
			if(const auto value = noteCache.restore(data)) {
				data->noteLineLayer = value->lineLayer.value__;
				data->cutDirection = value->cutDirection.value__;
			}
		} else if(GlobalNamespace::ObstacleData *const obstacleData = il2cpp_utils::try_cast<GlobalNamespace::ObstacleData>(item).value_or(nullptr)) {
			if(const std::optional<BeatmapSaveDataVersion2_6_0AndEarlier::ObstacleType> type = obstacleCache.restore(obstacleData)) {
				obstacleData->lineLayer = GetLayerForObstacleType(obstacleData->lineLayer, type->value__);
				obstacleData->height = GetHeightForObstacleType(obstacleData->height, type->value__);
			}
		} else if(GlobalNamespace::SliderData *const sliderData = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(item).value_or(nullptr)) {
			if(const auto layers = sliderCache.restore(sliderData)) {
				sliderData->headLineLayer = layers->headLayer.value__;
				sliderData->headBeforeJumpLineLayer = layers->headLayer.value__;
				sliderData->tailLineLayer = layers->tailLayer.value__;
				sliderData->tailBeforeJumpLineLayer = layers->tailLayer.value__;
			}
		} else if(GlobalNamespace::WaypointData *const waypointData = il2cpp_utils::try_cast<GlobalNamespace::WaypointData>(item).value_or(nullptr)) {
			if(const std::optional<BeatmapSaveDataCommon::NoteLineLayer> lineLayer = waypointCache.restore(waypointData))
				waypointData->lineLayer = lineLayer->value__;
		} else if(GlobalNamespace::SpawnRotationBeatmapEventData *const rotationData = il2cpp_utils::try_cast<GlobalNamespace::SpawnRotationBeatmapEventData>(item).value_or(nullptr)) {
			if(const std::optional<int32_t> rotation = rotationCache.restore(rotationData))
				rotationData->_deltaRotation = SpawnRotationForEventValue(rotationData->_deltaRotation, *rotation);
		}
		if(iter == end)
			break;
	}
	if(noteCache.failCount || obstacleCache.failCount || sliderCache.failCount || waypointCache.failCount)
		logger.warn("Failed to restore {} notes, {} obstacles, {} sliders, {} waypoints, and {} rotation events",
			noteCache.failCount, obstacleCache.failCount, sliderCache.failCount, waypointCache.failCount, rotationCache.failCount);
	return result.ptr();
}

MAKE_HOOK_MATCH(BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData, &BeatmapDataLoaderVersion3::BeatmapDataLoader::GetBeatmapDataFromSaveData, GlobalNamespace::BeatmapData*, BeatmapSaveDataVersion3::BeatmapSaveData *const beatmapSaveData, BeatmapSaveDataVersion4::LightshowSaveData *const defaultLightshowSaveData, GlobalNamespace::BeatmapDifficulty beatmapDifficulty, const float startBpm, bool loadingForDesignatedEnvironment, GlobalNamespace::EnvironmentKeywords *const environmentKeywords, GlobalNamespace::IEnvironmentLightGroups *const environmentLightGroups, GlobalNamespace::PlayerSpecificSettings *const playerSpecificSettings, System::Diagnostics::Stopwatch *const stopwatch) {
	if(!active)
		return BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData(beatmapSaveData, defaultLightshowSaveData, beatmapDifficulty,
			startBpm, loadingForDesignatedEnvironment, environmentKeywords, environmentLightGroups, playerSpecificSettings, stopwatch);
	const std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> bpmChanges = ConvertBpmChanges(startBpm, beatmapSaveData->bpmEvents);
	LayerCache bombCache(beatmapSaveData->__cordl_internal_get_bombNotes(), &bpmChanges);
	LayerCache noteCache(beatmapSaveData->__cordl_internal_get_colorNotes(), &bpmChanges);
	LayerCache obstacleCache(beatmapSaveData->__cordl_internal_get_obstacles(), &bpmChanges);
	LayerCache burstCache(beatmapSaveData->__cordl_internal_get_burstSliders(), &bpmChanges);
	LayerCache sliderCache(beatmapSaveData->__cordl_internal_get_sliders(), &bpmChanges);
	LayerCache waypointCache(beatmapSaveData->__cordl_internal_get_waypoints(), &bpmChanges);
	logger.info("Restoring {} notes, {} bombs, {} obstacles, {} sliders, {} burst sliders, and {} waypoints",
		noteCache.cache.size(), bombCache.cache.size(), obstacleCache.cache.size(), sliderCache.cache.size(), burstCache.cache.size(), waypointCache.cache.size());
	SafePtr<GlobalNamespace::BeatmapData> result = BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData(
		beatmapSaveData, defaultLightshowSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment,
		environmentKeywords, environmentLightGroups, playerSpecificSettings, stopwatch);
	for(System::Collections::Generic::LinkedListNode_1<GlobalNamespace::BeatmapDataItem*> *iter = result->get_allBeatmapDataItems()->head, *const end = iter ? iter->prev : nullptr; iter; iter = iter->next) {
		GlobalNamespace::BeatmapDataItem *const item = iter->item;
		if(GlobalNamespace::NoteData *const data = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(item).value_or(nullptr); data) {
			if(data->gameplayType == GlobalNamespace::NoteData::GameplayType::Bomb) {
				if(const std::optional<int32_t> lineLayer = bombCache.restore(data))
					data->noteLineLayer = *lineLayer;
			} else if(const auto value = noteCache.restore(data)) {
				data->noteLineLayer = value->layer.value__;
				data->cutDirection = value->cutDirection.value__;
			}
		} else if(GlobalNamespace::ObstacleData *const obstacleData = il2cpp_utils::try_cast<GlobalNamespace::ObstacleData>(item).value_or(nullptr)) {
			if(const std::optional<int32_t> lineLayer = obstacleCache.restore(obstacleData))
				obstacleData->lineLayer = *lineLayer;
		} else if(GlobalNamespace::SliderData *const sliderData = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(item).value_or(nullptr)) {
			if(const auto layers = (sliderData->sliderType == GlobalNamespace::SliderData::Type::Burst) ? burstCache.restore(sliderData) : sliderCache.restore(sliderData)) {
				sliderData->headLineLayer = layers->headLayer.value__;
				sliderData->headBeforeJumpLineLayer = layers->headLayer.value__;
				sliderData->tailLineLayer = layers->tailLayer.value__;
				sliderData->tailBeforeJumpLineLayer = layers->tailLayer.value__;
			}
		} else if(GlobalNamespace::WaypointData *const waypointData = il2cpp_utils::try_cast<GlobalNamespace::WaypointData>(item).value_or(nullptr)) {
			if(const std::optional<BeatmapSaveDataCommon::NoteLineLayer> lineLayer = waypointCache.restore(waypointData))
				waypointData->lineLayer = lineLayer->value__;
		}
		if(iter == end)
			break;
	}
	if(noteCache.failCount || bombCache.failCount || obstacleCache.failCount || sliderCache.failCount || burstCache.failCount || waypointCache.failCount)
		logger.warn("Failed to restore {} notes, {} bombs, {} obstacles, {} sliders, {} burst sliders, and {} waypoints",
			noteCache.failCount, bombCache.failCount, obstacleCache.failCount, sliderCache.failCount, burstCache.failCount, waypointCache.failCount);
	return result.ptr();
}

MAKE_HOOK_MATCH(BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, &GlobalNamespace::BeatmapObjectsInTimeRowProcessor::HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, void, GlobalNamespace::BeatmapObjectsInTimeRowProcessor *const self, GlobalNamespace::BeatmapObjectsInTimeRowProcessor::TimeSliceContainer_1<GlobalNamespace::BeatmapDataItem*>* allObjectsTimeSlice, float nextTimeSliceTime) {
	BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice(self, allObjectsTimeSlice, nextTimeSliceTime);
	if(!active)
		return;
	System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::BeatmapDataItem*> *const items = allObjectsTimeSlice->get_items();
	const uint32_t itemCount = static_cast<uint32_t>(
		reinterpret_cast<System::Collections::Generic::IReadOnlyCollection_1<GlobalNamespace::BeatmapDataItem*>*>(items)->get_Count());

	std::unordered_map<int32_t, std::vector<GlobalNamespace::NoteData*>> notesInColumnsProcessingDictionaryOfLists;
	for(uint32_t i = 0; i < itemCount; ++i) {
		GlobalNamespace::NoteData *const note = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(items->get_Item(static_cast<int32_t>(i))).value_or(nullptr);
		if(!note)
			continue;
		std::vector<GlobalNamespace::NoteData*> *const list = &notesInColumnsProcessingDictionaryOfLists.try_emplace(note->lineIndex).first->second;
		GlobalNamespace::NoteLineLayer lineLayer = note->noteLineLayer;
		std::vector<GlobalNamespace::NoteData*>::const_iterator pos = std::find_if(list->begin(), list->end(), [lineLayer](GlobalNamespace::NoteData *const e) {
			return e->noteLineLayer > lineLayer;
		});
		list->insert(pos, note);
	}
	for(std::pair<const int32_t, std::vector<GlobalNamespace::NoteData*>> &list : notesInColumnsProcessingDictionaryOfLists)
		for(uint32_t i = 0; i < static_cast<uint32_t>(list.second.size()); ++i)
			list.second[i]->SetBeforeJumpNoteLineLayer(static_cast<int32_t>(i));
	for(uint32_t i = 0; i < itemCount; ++i) {
		GlobalNamespace::SliderData *const slider = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(items->get_Item(static_cast<int32_t>(i))).value_or(nullptr);
		if(!slider)
			continue;
		for(uint32_t j = 0; j < itemCount; ++j) {
			GlobalNamespace::NoteData *const note = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(items->get_Item(static_cast<int32_t>(j))).value_or(nullptr);
			if(!note)
				continue;
			if(!GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderHeadPositionOverlapsWithNote(slider, note))
				continue;
			slider->SetHeadBeforeJumpLineLayer(note->beforeJumpNoteLineLayer);
		}
	}
	for(uint32_t i = 0; i < itemCount; ++i) {
		GlobalNamespace::SliderData *const slider = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(items->get_Item(static_cast<int32_t>(i))).value_or(nullptr);
		if(!slider)
			continue;
		for(uint32_t j = 0; j < itemCount; ++j) {
			GlobalNamespace::SliderData *const otherSlider = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(items->get_Item(static_cast<int32_t>(j))).value_or(nullptr);
			if(!otherSlider)
				continue;
			if(slider != otherSlider && GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderHeadPositionOverlapsWithBurstTail(slider, otherSlider))
				slider->SetHeadBeforeJumpLineLayer(otherSlider->tailBeforeJumpLineLayer);
		}
		for(uint32_t j = 0; j < itemCount; ++j) {
			GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderTailData *const tailData = il2cpp_utils::try_cast<GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderTailData>(items->get_Item(static_cast<int32_t>(j))).value_or(nullptr);
			if(!tailData)
				continue;
			if(GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderHeadPositionOverlapsWithBurstTail(slider, tailData->slider))
				slider->SetHeadBeforeJumpLineLayer(tailData->slider->tailBeforeJumpLineLayer);
		}
	}
	for(uint32_t i = 0; i < itemCount; ++i) {
		GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderTailData *const tailData = il2cpp_utils::try_cast<GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderTailData>(items->get_Item(static_cast<int32_t>(i))).value_or(nullptr);
		if(!tailData)
			continue;
		GlobalNamespace::SliderData *const slider = tailData->slider;
		for(uint32_t j = 0; j < itemCount; ++j) {
			GlobalNamespace::NoteData *const note = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(items->get_Item(static_cast<int32_t>(j))).value_or(nullptr);
			if(!note)
				continue;
			if(GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderTailPositionOverlapsWithNote(slider, note))
				slider->SetTailBeforeJumpLineLayer(note->beforeJumpNoteLineLayer);
		}
	}
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetNoteOffset, &GlobalNamespace::BeatmapObjectSpawnMovementData::GetNoteOffset, UnityEngine::Vector3, GlobalNamespace::BeatmapObjectSpawnMovementData *const self, int32_t noteLineIndex, const GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetNoteOffset(self, noteLineIndex, noteLineLayer);
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
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetObstacleOffset(self, noteLineIndex, noteLineLayer);
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

MAKE_HOOK_MATCH(NoteBasicCutInfoHelper_GetBasicCutInfo, &GlobalNamespace::NoteBasicCutInfoHelper::GetBasicCutInfo, void, UnityEngine::Transform *const noteTransform, GlobalNamespace::ColorType colorType, GlobalNamespace::NoteCutDirection cutDirection, GlobalNamespace::SaberType saberType, float saberBladeSpeed, UnityEngine::Vector3 cutDirVec, float cutAngleTolerance, ByRef<bool> directionOK, ByRef<bool> speedOK, ByRef<bool> saberTypeOK, ByRef<float> cutDirDeviation, ByRef<float> cutDirAngle) {
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

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Direction, &GlobalNamespace::NoteCutDirectionExtensions::Direction, UnityEngine::Vector2, GlobalNamespace::NoteCutDirection cutDirection) {
	UnityEngine::Vector2 result = NoteCutDirectionExtensions_Direction(cutDirection);
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

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_RotationAngle, &GlobalNamespace::NoteCutDirectionExtensions::RotationAngle, float, GlobalNamespace::NoteCutDirection cutDirection) {
	const float result = NoteCutDirectionExtensions_RotationAngle(cutDirection);
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

MAKE_HOOK_MATCH(ObstacleController_Init, &GlobalNamespace::ObstacleController::Init, void, GlobalNamespace::ObstacleController *const self, GlobalNamespace::ObstacleData *const obstacleData, float worldRotation, UnityEngine::Vector3 startPos, UnityEngine::Vector3 midPos, UnityEngine::Vector3 endPos, float move1Duration, float move2Duration, float singleLineWidth, float height) {
	if(!active)
		return ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth, height);
	if(obstacleData->height <= -1000)
		height = static_cast<float>(obstacleData->height + 2000) / 1000.f * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(obstacleData->height >= 1000)
		height = static_cast<float>(obstacleData->height - 1000) / 1000.f * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(obstacleData->height > 2)
		height = static_cast<float>(obstacleData->height) * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;

	int32_t oldWidth = obstacleData->width;
	if(oldWidth <= -1000)
		obstacleData->width = oldWidth + 1000;
	else if(oldWidth >= 1000)
		obstacleData->width = oldWidth - 1000;
	else
		return ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth, height);
	float fix = singleLineWidth * (-999.f / 1000) * .5f;
	midPos.x += fix;
	endPos.x += fix;
	ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth / 1000, height);
	self->_startPos.x += fix;
	obstacleData->width = oldWidth;
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
	UnityEngine::Vector3 result = SliderMeshController_CutDirectionToControlPointPosition(noteCutDirection);
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

MAKE_HOOK_MATCH(StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer, &GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer, float, GlobalNamespace::NoteLineLayer lineLayer) {
	float result = StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer(lineLayer);
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
		.version = "0.23.1",
		.version_long = 13,
	};
	logger.info("Leaving setup!");
}

extern "C" void late_load();
extern "C" [[gnu::visibility("default")]] void late_load() {
	il2cpp_functions::Init();

	INSTALL_HOOK(logger, GameplayCoreSceneSetupData_LoadTransformedBeatmapDataAsync)
	INSTALL_HOOK(logger, GameplayCoreSceneSetupData_LoadTransformedBeatmapData)
	if(const std::vector<modloader::ModData> loaded = modloader::get_loaded(); std::find_if(loaded.begin(), loaded.end(), [](const modloader::ModData &data) {
			return data.info.id == "CustomJSONData";
		}) == loaded.end()) {
		INSTALL_HOOK(logger, BeatmapDataLoaderVersion2_6_0AndEarlier_BeatmapDataLoader_GetBeatmapDataFromSaveData)
		INSTALL_HOOK(logger, BeatmapDataLoaderVersion3_BeatmapDataLoader_GetBeatmapDataFromSaveData)
		// INSTALL_HOOK(logger, BeatmapDataLoaderVersion4_BeatmapDataLoader_GetBeatmapDataFromSaveData) // TODO: implement
	}

	INSTALL_HOOK(logger, BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice)
	INSTALL_HOOK(logger, BeatmapObjectSpawnMovementData_GetNoteOffset)
	INSTALL_HOOK(logger, StaticBeatmapObjectSpawnMovementData_Get2DNoteOffset)
	INSTALL_HOOK(logger, BeatmapObjectSpawnMovementData_GetObstacleOffset)
	INSTALL_HOOK(logger, BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer)
	// [HarmonyPatch(typeof(ColorNoteVisuals), nameof(ColorNoteVisuals.HandleNoteControllerDidInit))]
	INSTALL_HOOK(logger, NoteBasicCutInfoHelper_GetBasicCutInfo)
	INSTALL_HOOK(logger, NoteCutDirectionExtensions_Rotation)
	INSTALL_HOOK(logger, NoteCutDirectionExtensions_Direction)
	INSTALL_HOOK(logger, NoteCutDirectionExtensions_RotationAngle)
	INSTALL_HOOK(logger, NoteCutDirectionExtensions_Mirrored)
	INSTALL_HOOK(logger, NoteData_Mirror)
	INSTALL_HOOK(logger, ObstacleController_Init)
	INSTALL_HOOK(logger, ObstacleData_Mirror)
	INSTALL_HOOK(logger, SliderData_Mirror)
	INSTALL_HOOK(logger, SliderMeshController_CutDirectionToControlPointPosition)
	INSTALL_HOOK(logger, StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer)

	logger.info("Installed ME Hooks successfully!");
	for(const std::string_view name : requirementNames)
		SongCore::API::Capabilities::RegisterCapability(name);
}

#include <GlobalNamespace/zzzz__BeatmapData_impl.hpp>
#include <GlobalNamespace/zzzz__BpmTimeProcessor_impl.hpp>
#include <GlobalNamespace/zzzz__IJumpOffsetYProvider_impl.hpp>
#include <GlobalNamespace/zzzz__SpawnRotationBeatmapEventData_impl.hpp>
#include <System/Collections/Generic/zzzz__IReadOnlyCollection_1_impl.hpp>
#include <System/Collections/Generic/zzzz__IReadOnlyList_1_impl.hpp>
#include <System/Collections/Generic/zzzz__LinkedList_1_impl.hpp>
#include <System/Collections/Generic/zzzz__LinkedListNode_1_impl.hpp>
