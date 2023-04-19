// https://github.com/rxzz0/MappingExtensions/blob/main/src/main.cpp
// Refactored and updated to 1.24.0 by rcelyte

#include <array>
#include <limits>
#include <map>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>
#include <string>
#include <optional>

#include "modloader/shared/modloader.hpp"

#include "beatsaber-hook/shared/utils/typedefs.h"
#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/logging.hpp"
#include "beatsaber-hook/shared/utils/utils.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveData_EventData.hpp>
#include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveData_ObstacleData.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_BeatmapSaveDataItem.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_BombNoteData.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_BurstSliderData.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_ColorNoteData.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_ObstacleData.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_RotationEventData.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_SliderData.hpp>
#include <BeatmapSaveDataVersion3/BeatmapSaveData_WaypointData.hpp>
#include <GlobalNamespace/BeatmapCharacteristicSO.hpp>
#include <GlobalNamespace/BeatmapData.hpp>
#include <GlobalNamespace/BeatmapDataLoader.hpp>
#include <GlobalNamespace/BeatmapDataLoader_ObstacleConvertor.hpp>
#include <GlobalNamespace/BeatmapDifficulty.hpp>
#include <GlobalNamespace/BeatmapEventData.hpp>
#include <GlobalNamespace/BeatmapLineData.hpp>
#include <GlobalNamespace/BeatmapObjectData.hpp>
#include <GlobalNamespace/BeatmapObjectExecutionRatingsRecorder.hpp>
#include <GlobalNamespace/BeatmapObjectsInTimeRowProcessor.hpp>
#include <GlobalNamespace/BeatmapObjectsInTimeRowProcessor_SliderTailData.hpp>
#include <GlobalNamespace/BeatmapObjectsInTimeRowProcessor_TimeSliceContainer_1.hpp>
#include <GlobalNamespace/BeatmapObjectSpawnController.hpp>
#include <GlobalNamespace/BeatmapObjectSpawnController_InitData.hpp>
#include <GlobalNamespace/BeatmapObjectSpawnMovementData.hpp>
#include <GlobalNamespace/FlyingScoreSpawner.hpp>
#include <GlobalNamespace/GameplayCoreSceneSetupData.hpp>
#include <GlobalNamespace/IDifficultyBeatmap.hpp>
#include <GlobalNamespace/IDifficultyBeatmapSet.hpp>
#include <GlobalNamespace/IJumpOffsetYProvider.hpp>
#include <GlobalNamespace/MainMenuViewController.hpp>
#include <GlobalNamespace/NoteBasicCutInfoHelper.hpp>
#include <GlobalNamespace/NoteCutDirectionExtensions.hpp>
#include <GlobalNamespace/NoteData.hpp>
#include <GlobalNamespace/NoteJump.hpp>
#include <GlobalNamespace/NoteLineLayer.hpp>
#include <GlobalNamespace/ObstacleController.hpp>
#include <GlobalNamespace/ObstacleData.hpp>
#include <GlobalNamespace/SaberType.hpp>
#include <GlobalNamespace/SimpleColorSO.hpp>
#include <GlobalNamespace/SliderMeshController.hpp>
#include <GlobalNamespace/StandardLevelDetailView.hpp>
#include <GlobalNamespace/StaticBeatmapObjectSpawnMovementData.hpp>
#include <GlobalNamespace/StretchableObstacle.hpp>
#include <GlobalNamespace/Vector2Extensions.hpp>
#include <GlobalNamespace/WaypointData.hpp>
#include <System/Collections/Generic/LinkedList_1.hpp>
#include <System/Collections/Generic/LinkedListNode_1.hpp>
#include <System/Collections/Generic/List_1.hpp>
#include <System/Decimal.hpp>
#include <System/Single.hpp>
#include <UnityEngine/Camera.hpp>
#include <UnityEngine/Color.hpp>
#include <UnityEngine/Graphics.hpp>
#include <UnityEngine/Resources.hpp>
#include <sombrero/shared/FastQuaternion.hpp>
#include <sombrero/shared/FastVector2.hpp>
#include <sombrero/shared/FastVector3.hpp>
#include <pinkcore/shared/API.hpp>
#include <pinkcore/shared/RequirementAPI.hpp>
#define DL_EXPORT __attribute__((visibility("default")))

static ModInfo modInfo;
static Logger *logger = NULL;

static int ToNormalizedPrecisionIndex(int index) {
	if(index <= -1000)
		return index + 1000;
	if(index >= 1000)
		return index - 1000;
	return index * 1000;
}

static const std::array<const char*, 4> requirementNames = {
	"Mapping Extensions",
	"Mapping Extensions-Precision Placement",
	"Mapping Extensions-Extra Note Angles",
	"Mapping Extensions-More Lanes",
};

namespace SongUtils {
	namespace CustomData {
		bool GetInfoJson(GlobalNamespace::IPreviewBeatmapLevel* level, std::shared_ptr<rapidjson::GenericDocument<rapidjson::UTF16<char16_t>>>& doc);
		bool GetCustomDataJsonFromDifficultyAndCharacteristic(rapidjson::GenericDocument<rapidjson::UTF16<char16_t>>& in, rapidjson::GenericValue<rapidjson::UTF16<char16_t>>& out, GlobalNamespace::BeatmapDifficulty difficulty, GlobalNamespace::BeatmapCharacteristicSO* characteristic);
		void ExtractRequirements(const rapidjson::GenericValue<rapidjson::UTF16<char16_t>>& requirementsArray, std::vector<std::string>& output);
	}
}

static bool ShouldActivate(GlobalNamespace::GameplayCoreSceneSetupData *data) {
	if(!data || !data->difficultyBeatmap)
		return false;
	GlobalNamespace::IDifficultyBeatmapSet *beatmapSet = data->difficultyBeatmap->get_parentDifficultyBeatmapSet();
	if(!beatmapSet)
		return false;
	std::shared_ptr<rapidjson::GenericDocument<rapidjson::UTF16<char16_t>>> info;
	if(!SongUtils::CustomData::GetInfoJson(data->previewBeatmapLevel, info))
		return false;
	rapidjson::GenericValue<rapidjson::UTF16<char16_t>> customData;
	if(!SongUtils::CustomData::GetCustomDataJsonFromDifficultyAndCharacteristic(*info, customData, data->difficultyBeatmap->get_difficulty(), beatmapSet->get_beatmapCharacteristic()))
		return false;
	rapidjson::GenericValue<rapidjson::UTF16<char16_t>>::MemberIterator requirements = customData.FindMember(u"_requirements");
	if(requirements == customData.MemberEnd())
		return false;
	std::vector<std::string> u8requirements;
	SongUtils::CustomData::ExtractRequirements(requirements->value, u8requirements);
	return std::any_of(u8requirements.begin(), u8requirements.end(), [](const std::string &req) {
		return std::any_of(requirementNames.begin(), requirementNames.end(), [req](const char *name) {
			return req == name;
		});
	});
}

static bool active = false;
MAKE_HOOK_MATCH(GameplayCoreSceneSetupData_GetTransformedBeatmapDataAsync, &GlobalNamespace::GameplayCoreSceneSetupData::GetTransformedBeatmapDataAsync, System::Threading::Tasks::Task_1<::GlobalNamespace::IReadonlyBeatmapData*>*, GlobalNamespace::GameplayCoreSceneSetupData *self) {
	active = ShouldActivate(self);
	logger->info("ShouldActivate(): %hhu", active);
	return GameplayCoreSceneSetupData_GetTransformedBeatmapDataAsync(self);
}

/* PC version hooks */

#include "conversions.hpp"
MAKE_HOOK_MATCH(BeatmapDataLoader_GetBeatmapDataFromBeatmapSaveData, &GlobalNamespace::BeatmapDataLoader::GetBeatmapDataFromBeatmapSaveData, GlobalNamespace::BeatmapData*, ::BeatmapSaveDataVersion3::BeatmapSaveData* beatmapSaveData, ::GlobalNamespace::BeatmapDifficulty beatmapDifficulty, float startBpm, bool loadingForDesignatedEnvironment, ::GlobalNamespace::EnvironmentKeywords* environmentKeywords, ::GlobalNamespace::EnvironmentLightGroups* environmentLightGroups, ::GlobalNamespace::DefaultEnvironmentEvents* defaultEnvironmentEvents, ::GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings) {
	if(!active)
		return BeatmapDataLoader_GetBeatmapDataFromBeatmapSaveData(beatmapSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment, environmentKeywords, environmentLightGroups, defaultEnvironmentEvents, playerSpecificSettings);
	LayerCache<BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData> bombCache(beatmapSaveData->bombNotes, startBpm, beatmapSaveData->bpmEvents);
	LayerCache<BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData> noteCache(beatmapSaveData->colorNotes, startBpm, beatmapSaveData->bpmEvents);
	LayerCache<BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData> obstacleCache(beatmapSaveData->obstacles, startBpm, beatmapSaveData->bpmEvents);
	LayerCache<BeatmapSaveDataVersion3::BeatmapSaveData::BurstSliderData, 2> burstCache(beatmapSaveData->burstSliders, startBpm, beatmapSaveData->bpmEvents);
	LayerCache<BeatmapSaveDataVersion3::BeatmapSaveData::SliderData, 2> sliderCache(beatmapSaveData->sliders, startBpm, beatmapSaveData->bpmEvents);
	LayerCache<BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData> waypointCache(beatmapSaveData->waypoints, startBpm, beatmapSaveData->bpmEvents);
	logger->info("Restoring %zu notes, %zu bombs, %zu obstacles, %zu sliders, %zu burst sliders, and %zu waypoints",
		noteCache.cache.size(), bombCache.cache.size(), obstacleCache.cache.size(), sliderCache.cache.size(), burstCache.cache.size(), waypointCache.cache.size());
	GlobalNamespace::BeatmapData *result = BeatmapDataLoader_GetBeatmapDataFromBeatmapSaveData(beatmapSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment, environmentKeywords, environmentLightGroups, defaultEnvironmentEvents, playerSpecificSettings);
	for(System::Collections::Generic::LinkedListNode_1<GlobalNamespace::BeatmapDataItem*> *iter = result->get_allBeatmapDataItems()->head, *end = iter ? iter->prev : NULL; iter; iter = iter->next) {
		GlobalNamespace::BeatmapDataItem *item = iter->item;
		if(GlobalNamespace::NoteData *data = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(item).value_or(nullptr); data) {
			data->noteLineLayer = (data->gameplayType == GlobalNamespace::NoteData::GameplayType::Bomb) ? bombCache.restore(data)[0] : noteCache.restore(data)[0];
		} else if(GlobalNamespace::ObstacleData *data = il2cpp_utils::try_cast<GlobalNamespace::ObstacleData>(item).value_or(nullptr); data) {
			data->lineLayer = obstacleCache.restore(data)[0];
		} else if(GlobalNamespace::SliderData *data = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(item).value_or(nullptr); data) {
			std::array<int32_t, 2> layers = (data->sliderType == GlobalNamespace::SliderData::Type::Burst) ? burstCache.restore(data) : sliderCache.restore(data);
			data->headBeforeJumpLineLayer = data->headLineLayer = layers[0];
			data->tailBeforeJumpLineLayer = data->tailLineLayer = layers[1];
		} else if(GlobalNamespace::WaypointData *data = il2cpp_utils::try_cast<GlobalNamespace::WaypointData>(item).value_or(nullptr); data) {
			data->lineLayer = waypointCache.restore(data)[0];
		}
		if(iter == end)
			break;
	}
	if(noteCache.failCount || bombCache.failCount || obstacleCache.failCount || sliderCache.failCount || burstCache.failCount || waypointCache.failCount)
		logger->warning("Failed to restore %zu notes, %zu bombs, %zu obstacles, %zu sliders, %zu burst sliders, and %zu waypoints",
			noteCache.failCount, bombCache.failCount, obstacleCache.failCount, sliderCache.failCount, burstCache.failCount, waypointCache.failCount);
	return result;
}

static inline bool SliderHeadPositionOverlapsWithNote(GlobalNamespace::SliderData *slider, GlobalNamespace::NoteData *note) {
	return slider->headLineIndex == note->lineIndex && slider->headLineLayer == note->noteLineLayer;
}
static inline bool SliderTailPositionOverlapsWithNote(GlobalNamespace::SliderData *slider, GlobalNamespace::NoteData *note) {
	return slider->tailLineIndex == note->lineIndex && slider->tailLineLayer == note->noteLineLayer;
}
MAKE_HOOK_MATCH(BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, &GlobalNamespace::BeatmapObjectsInTimeRowProcessor::HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, void, GlobalNamespace::BeatmapObjectsInTimeRowProcessor *self, ::GlobalNamespace::BeatmapObjectsInTimeRowProcessor::TimeSliceContainer_1<::GlobalNamespace::BeatmapDataItem*>* allObjectsTimeSlice, float nextTimeSliceTime) {
	BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice(self, allObjectsTimeSlice, nextTimeSliceTime);
	if(!active)
		return;
	System::Collections::Generic::IReadOnlyList_1<GlobalNamespace::BeatmapDataItem*> *items = allObjectsTimeSlice->get_items();
	uint32_t itemCount = ((System::Collections::Generic::IReadOnlyCollection_1<GlobalNamespace::BeatmapDataItem*>*)items)->get_Count();

	std::unordered_map<int32_t, std::vector<GlobalNamespace::NoteData*>> notesInColumnsReusableProcessingDictionaryOfLists;
	for(uint32_t i = 0; i < itemCount; ++i) {
		GlobalNamespace::NoteData *note = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(items->get_Item(i)).value_or(nullptr);
		if(!note)
			continue;
		std::vector<GlobalNamespace::NoteData*> *list = &notesInColumnsReusableProcessingDictionaryOfLists.try_emplace(note->lineIndex).first->second;
		GlobalNamespace::NoteLineLayer lineLayer = note->noteLineLayer;
		std::vector<GlobalNamespace::NoteData*>::const_iterator pos = std::find_if(list->begin(), list->end(), [lineLayer](GlobalNamespace::NoteData *e) {
			return e->noteLineLayer > lineLayer;
		});
		list->insert(pos, note);
	}
	for(std::pair<const int, std::vector<GlobalNamespace::NoteData*>> &list : notesInColumnsReusableProcessingDictionaryOfLists)
		for(int i = 0; i < list.second.size(); ++i)
			list.second[i]->SetBeforeJumpNoteLineLayer(i);
	for(uint32_t i = 0; i < itemCount; ++i) {
		GlobalNamespace::SliderData *slider = il2cpp_utils::try_cast<GlobalNamespace::SliderData>(items->get_Item(i)).value_or(nullptr);
		if(!slider)
			continue;
		for(uint32_t j = 0; j < itemCount; ++j) {
			GlobalNamespace::NoteData *note = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(items->get_Item(j)).value_or(nullptr);
			if(!note)
				continue;
			if(!SliderHeadPositionOverlapsWithNote(slider, note))
				continue;
			slider->SetHasHeadNote(true);
			slider->SetHeadBeforeJumpLineLayer(note->beforeJumpNoteLineLayer);
			if(slider->sliderType != GlobalNamespace::SliderData::Type::Burst) {
				note->ChangeToSliderHead();
				continue;
			}
			note->ChangeToBurstSliderHead();
			if(note->cutDirection != slider->tailCutDirection)
				continue;
			UnityEngine::Vector2 line = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::Get2DNoteOffset(note->lineIndex, self->numberOfLines, note->noteLineLayer) - GlobalNamespace::StaticBeatmapObjectSpawnMovementData::Get2DNoteOffset(slider->tailLineIndex, self->numberOfLines, slider->tailLineLayer);
			float num = GlobalNamespace::Vector2Extensions::SignedAngleToLine(GlobalNamespace::NoteCutDirectionExtensions::Direction(note->cutDirection), line);
			if(abs(num) > 40)
				continue;
			note->SetCutDirectionAngleOffset(num);
			slider->SetCutDirectionAngleOffset(num, num);
		}
	}
	for(uint32_t i = 0; i < itemCount; ++i) {
		GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderTailData *tailData = il2cpp_utils::try_cast<GlobalNamespace::BeatmapObjectsInTimeRowProcessor::SliderTailData>(items->get_Item(i)).value_or(nullptr);
		if(!tailData)
			continue;
		GlobalNamespace::SliderData *slider = tailData->slider;
		for(uint32_t j = 0; j < itemCount; ++j) {
			GlobalNamespace::NoteData *note = il2cpp_utils::try_cast<GlobalNamespace::NoteData>(items->get_Item(j)).value_or(nullptr);
			if(!note)
				continue;
			if(!SliderTailPositionOverlapsWithNote(slider, note))
				continue;
			slider->SetHasTailNote(true);
			slider->SetTailBeforeJumpLineLayer(note->beforeJumpNoteLineLayer);
			note->ChangeToSliderTail();
		}
	}
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetNoteOffset, &GlobalNamespace::BeatmapObjectSpawnMovementData::GetNoteOffset, UnityEngine::Vector3, GlobalNamespace::BeatmapObjectSpawnMovementData* self, int noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetNoteOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	num += noteLineIndex * (GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f);
	return Sombrero::FastVector3(self->rightVec) * num + Sombrero::FastVector3(0, GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer), 0);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_Get2DNoteOffset, &GlobalNamespace::BeatmapObjectSpawnMovementData::Get2DNoteOffset, UnityEngine::Vector2, GlobalNamespace::BeatmapObjectSpawnMovementData* self, int noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector2 result = BeatmapObjectSpawnMovementData_Get2DNoteOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	float x = num + noteLineIndex * (GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f);
	float y = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer);
	return UnityEngine::Vector2(x, y);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetObstacleOffset, &GlobalNamespace::BeatmapObjectSpawnMovementData::GetObstacleOffset, UnityEngine::Vector3, GlobalNamespace::BeatmapObjectSpawnMovementData *self, int noteLineIndex, ::GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetObstacleOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	num += noteLineIndex * (GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f);
	return Sombrero::FastVector3(self->rightVec) * num + Sombrero::FastVector3(0, GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer) + GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kObstacleVerticalOffset, 0);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer, &GlobalNamespace::BeatmapObjectSpawnMovementData::HighestJumpPosYForLineLayer, float, GlobalNamespace::BeatmapObjectSpawnMovementData* self, GlobalNamespace::NoteLineLayer lineLayer) {
	float result = BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer(self, lineLayer);
	if(!active)
		return result;
	float delta = (self->topLinesHighestJumpPosY - self->upperLinesHighestJumpPosY);
	if(lineLayer >= 1000 || lineLayer <= -1000)
		return self->upperLinesHighestJumpPosY - delta - delta + self->jumpOffsetYProvider->get_jumpOffsetY() + lineLayer * (delta / 1000.f);
	if(lineLayer > 2 || lineLayer < 0)
		return self->upperLinesHighestJumpPosY - delta + self->jumpOffsetYProvider->get_jumpOffsetY() + (lineLayer * delta);
	return result;
}

static inline float SpawnRotationForEventValue(float orig, int index) {
	if(index >= 1000 && index <= 1720)
		return index - 1360;
	return orig;
}

static inline int GetHeightForObstacleType(int orig, BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::ObstacleType obstacleType) {
	if((obstacleType < 1000 || obstacleType > 4000) && (obstacleType < 4001 || obstacleType > 4005000))
		return orig;
	int32_t obsHeight = obstacleType - 1000;
	if(obstacleType >= 4001 && obstacleType <= 4100000)
		obsHeight = (obstacleType - 4001) / 1000;
	return obsHeight * 5 + 1000;
}

static inline int GetLayerForObstacleType(int orig, BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::ObstacleType obstacleType) {
	if((obstacleType < 1000 || obstacleType > 4000) && (obstacleType < 4001 || obstacleType > 4005000))
		return orig;
	int32_t startHeight = 0;
	if(obstacleType >= 4001 && obstacleType <= 4100000)
		startHeight = (obstacleType - 4001) % 1000;
	float layer = startHeight / 750.f * 5;
	return layer * 1000 + 1334;
}

MAKE_HOOK_MATCH(BeatmapSaveData_ConvertBeatmapSaveData, &BeatmapSaveDataVersion3::BeatmapSaveData::ConvertBeatmapSaveData, BeatmapSaveDataVersion3::BeatmapSaveData*, BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData *beatmapSaveData) {
	BeatmapSaveDataVersion3::BeatmapSaveData *result = BeatmapSaveData_ConvertBeatmapSaveData(beatmapSaveData);
	logger->info("BeatmapSaveData_ConvertBeatmapSaveData()");
	for(uint32_t i = 0, count = result->obstacles->get_Count(); i < count; ++i) {
		BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::ObstacleData *v2 = beatmapSaveData->obstacles->get_Item(i);
		BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData *v3 = result->obstacles->get_Item(i);
		v3->y = GetLayerForObstacleType(v3->y, v2->type);
		v3->h = GetHeightForObstacleType(v3->h, v2->type);
	}
	if(!beatmapSaveData->events)
		return result;
	for(uint32_t i = 0, j = 0, count = beatmapSaveData->events->get_Count(); i < count; ++i) {
		BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::EventData *v2 = beatmapSaveData->events->get_Item(i);
		if(v2->type != BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::BeatmapEventType::Event14 && v2->type != BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::BeatmapEventType::Event15)
			continue;
		BeatmapSaveDataVersion3::BeatmapSaveData::RotationEventData *v3 = result->rotationEvents->get_Item(j++);
		v3->r = SpawnRotationForEventValue(v3->r, v2->value);
	}
	return result;
}

MAKE_HOOK_MATCH(NoteBasicCutInfoHelper_GetBasicCutInfo, &GlobalNamespace::NoteBasicCutInfoHelper::GetBasicCutInfo, void, ::UnityEngine::Transform* noteTransform, ::GlobalNamespace::ColorType colorType, ::GlobalNamespace::NoteCutDirection cutDirection, ::GlobalNamespace::SaberType saberType, float saberBladeSpeed, ::UnityEngine::Vector3 cutDirVec, float cutAngleTolerance, ByRef<bool> directionOK, ByRef<bool> speedOK, ByRef<bool> saberTypeOK, ByRef<float> cutDirDeviation, ByRef<float> cutDirAngle) {
	if(active && cutDirection >= 2000 && cutDirection <= 2360)
		cutDirection = GlobalNamespace::NoteCutDirection::Any;
	NoteBasicCutInfoHelper_GetBasicCutInfo(noteTransform, colorType, cutDirection, saberType, saberBladeSpeed, cutDirVec, cutAngleTolerance, directionOK, speedOK, saberTypeOK, cutDirDeviation, cutDirAngle);
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Rotation, &GlobalNamespace::NoteCutDirectionExtensions::Rotation, UnityEngine::Quaternion, GlobalNamespace::NoteCutDirection cutDirection, float offset) {
	UnityEngine::Quaternion result = NoteCutDirectionExtensions_Rotation(cutDirection, offset);
	if(!active)
		return result;
	if(cutDirection >= 1000 && cutDirection <= 1360) {
		result = UnityEngine::Quaternion();
		result.set_eulerAngles(UnityEngine::Vector3(0, 0, 1000 - cutDirection));
	} else if(cutDirection >= 2000 && cutDirection <= 2360) {
		result = UnityEngine::Quaternion();
		result.set_eulerAngles(UnityEngine::Vector3(0, 0, 2000 - cutDirection));
	}
	return result;
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Direction, &GlobalNamespace::NoteCutDirectionExtensions::Direction, UnityEngine::Vector2, GlobalNamespace::NoteCutDirection cutDirection) {
	UnityEngine::Vector2 result = NoteCutDirectionExtensions_Direction(cutDirection);
	if(!active)
		return result;
	int32_t offset = 2000;
	if(cutDirection >= 1000 && cutDirection <= 1360)
		offset = 1000;
	else if(cutDirection < 2000 || cutDirection > 2360)
		return result;
	Sombrero::FastQuaternion quaternion = Sombrero::FastQuaternion();
	quaternion.set_eulerAngles(UnityEngine::Vector3(0, 0, offset - cutDirection));
	Sombrero::FastVector3 dir = quaternion * Sombrero::FastVector3::down();
	return UnityEngine::Vector2(dir.x, dir.y);
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_RotationAngle, &GlobalNamespace::NoteCutDirectionExtensions::RotationAngle, float, GlobalNamespace::NoteCutDirection cutDirection) {
	float result = NoteCutDirectionExtensions_RotationAngle(cutDirection);
	if(!active)
		return result;
	if(cutDirection >= 1000 && cutDirection <= 1360)
		return 1000 - cutDirection;
	if(cutDirection >= 2000 && cutDirection <= 2360)
		return 2000 - cutDirection;
	return result;
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Mirrored, &GlobalNamespace::NoteCutDirectionExtensions::Mirrored, GlobalNamespace::NoteCutDirection, GlobalNamespace::NoteCutDirection cutDirection) {
	GlobalNamespace::NoteCutDirection result = NoteCutDirectionExtensions_Mirrored(cutDirection);
	if(!active)
		return result;
	if(cutDirection >= 1000 && cutDirection <= 1360)
		return 2360 - cutDirection;
	if(cutDirection >= 2000 && cutDirection <= 2360)
		return 4360 - cutDirection;
	return result;
}

static inline bool MirrorPrecisionLineIndex(int32_t *lineIndex) {
	if(*lineIndex >= 1000 || *lineIndex <= -1000) {
		*lineIndex = ((*lineIndex > -1000 && *lineIndex < 4000) ? 5000 : 3000) - *lineIndex;
		return true;
	}
	if(*lineIndex > 3 || *lineIndex < 0) {
		*lineIndex = 3 - *lineIndex;
		return true;
	}
	return false;
}

MAKE_HOOK_MATCH(NoteData_Mirror, &GlobalNamespace::NoteData::Mirror, void, GlobalNamespace::NoteData* self, int lineCount) {
	int32_t lineIndex = self->lineIndex;
	int32_t flipLineIndex = self->flipLineIndex;
	NoteData_Mirror(self, lineCount);
	if(!active)
		return;
	if(MirrorPrecisionLineIndex(&lineIndex))
		self->set_lineIndex(lineIndex);
	if(MirrorPrecisionLineIndex(&flipLineIndex))
		self->set_flipLineIndex(flipLineIndex);
}

MAKE_HOOK_MATCH(ObstacleController_Init, &GlobalNamespace::ObstacleController::Init, void, GlobalNamespace::ObstacleController* self, GlobalNamespace::ObstacleData* obstacleData, float worldRotation, UnityEngine::Vector3 startPos, UnityEngine::Vector3 midPos, UnityEngine::Vector3 endPos, float move1Duration, float move2Duration, float singleLineWidth, float height) {
	if(!active)
		return ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth, height);
	if(obstacleData->height <= -1000)
		height = (obstacleData->height + 2000) / 1000.f * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(obstacleData->height >= 1000)
		height = (obstacleData->height - 1000) / 1000.f * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(obstacleData->height > 2)
		height = obstacleData->height * GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;

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
	self->startPos.x += fix;
	obstacleData->width = oldWidth;
}

MAKE_HOOK_MATCH(ObstacleData_Mirror, &GlobalNamespace::ObstacleData::Mirror, void, GlobalNamespace::ObstacleData* self, int lineCount) {
	int32_t lineIndex = self->lineIndex;
	ObstacleData_Mirror(self, lineCount);
	if(!active)
		return;
	if(lineIndex >= 1000 || lineIndex <= -1000 || self->width >= 1000 || self->width <= -1000) {
		int32_t newIndex = 4000 - ToNormalizedPrecisionIndex(lineIndex);
		int32_t newWidth = ToNormalizedPrecisionIndex(self->width);
		newIndex -= newWidth;
		self->lineIndex = (newIndex < 0) ? newIndex - 1000 : newIndex + 1000;
	} else if(lineIndex < 0 || lineIndex > 3) {
		int32_t mirrorLane = 4 - lineIndex;
		self->lineIndex = mirrorLane - self->width;
	}
}

MAKE_HOOK_MATCH(SliderData_Mirror, &GlobalNamespace::SliderData::Mirror, void, GlobalNamespace::SliderData *self, int lineCount) {
	int32_t headLineIndex = self->headLineIndex;
	int32_t tailLineIndex = self->tailLineIndex;
	SliderData_Mirror(self, lineCount);
	if(!active)
		return;
	if(MirrorPrecisionLineIndex(&headLineIndex))
		self->headLineIndex = headLineIndex;
	if(MirrorPrecisionLineIndex(&tailLineIndex))
		self->tailLineIndex = tailLineIndex;
}

MAKE_HOOK_MATCH(SliderMeshController_CutDirectionToControlPointPosition, &GlobalNamespace::SliderMeshController::CutDirectionToControlPointPosition, UnityEngine::Vector3, GlobalNamespace::NoteCutDirection noteCutDirection) {
	UnityEngine::Vector3 result = SliderMeshController_CutDirectionToControlPointPosition(noteCutDirection);
	if(!active)
		return result;
	if(noteCutDirection >= 1000 && noteCutDirection <= 1360) {
		Sombrero::FastQuaternion quaternion = Sombrero::FastQuaternion();
		quaternion.set_eulerAngles(UnityEngine::Vector3(0, 0, 1000 - noteCutDirection));
		return quaternion * Sombrero::FastVector3::down();
	}
	if(noteCutDirection >= 2000 && noteCutDirection <= 2360) {
		Sombrero::FastQuaternion quaternion = Sombrero::FastQuaternion();
		quaternion.set_eulerAngles(UnityEngine::Vector3(0, 0, 2000 - noteCutDirection));
		return quaternion * Sombrero::FastVector3::down();
	}
	return result;
}

MAKE_HOOK_MATCH(StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer, &GlobalNamespace::StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer, float, GlobalNamespace::NoteLineLayer lineLayer) {
	float result = StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer(lineLayer);
	if(!active)
		return result;
	constexpr float delta = GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kTopLinesYPos - GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos;
	if(lineLayer >= 1000 || lineLayer <= -1000)
		return GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta - delta + lineLayer * (delta / 1000.f);
	if(lineLayer > 2 || lineLayer < 0)
		return GlobalNamespace::StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta + lineLayer * delta;
	return result;
}

extern "C" DL_EXPORT void setup(ModInfo& info) {
	info.id = "MappingExtensions";
	info.version = "0.22.3";
	modInfo = info;
	logger = new Logger(modInfo, LoggerOptions(false, true));
	logger->info("Leaving setup!");
}

extern "C" DL_EXPORT void load() {
	logger->info("Installing ME Hooks, please wait");
	il2cpp_functions::Init();

	INSTALL_HOOK(*logger, GameplayCoreSceneSetupData_GetTransformedBeatmapDataAsync);
	INSTALL_HOOK(*logger, BeatmapDataLoader_GetBeatmapDataFromBeatmapSaveData);
	INSTALL_HOOK(*logger, BeatmapSaveData_ConvertBeatmapSaveData);

	INSTALL_HOOK(*logger, BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice);
	INSTALL_HOOK(*logger, BeatmapObjectSpawnMovementData_GetNoteOffset);
	INSTALL_HOOK(*logger, BeatmapObjectSpawnMovementData_Get2DNoteOffset);
	INSTALL_HOOK(*logger, BeatmapObjectSpawnMovementData_GetObstacleOffset);
	INSTALL_HOOK(*logger, BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer);
	// [HarmonyPatch(typeof(ColorNoteVisuals), nameof(ColorNoteVisuals.HandleNoteControllerDidInit))]
	INSTALL_HOOK(*logger, NoteBasicCutInfoHelper_GetBasicCutInfo);
	INSTALL_HOOK(*logger, NoteCutDirectionExtensions_Rotation);
	INSTALL_HOOK(*logger, NoteCutDirectionExtensions_Direction);
	INSTALL_HOOK(*logger, NoteCutDirectionExtensions_RotationAngle);
	INSTALL_HOOK(*logger, NoteCutDirectionExtensions_Mirrored);
	INSTALL_HOOK(*logger, NoteData_Mirror);
	INSTALL_HOOK(*logger, ObstacleController_Init);
	INSTALL_HOOK(*logger, ObstacleData_Mirror);
	INSTALL_HOOK(*logger, SliderData_Mirror);
	INSTALL_HOOK(*logger, SliderMeshController_CutDirectionToControlPointPosition);
	INSTALL_HOOK(*logger, StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer);

	logger->info("Installed ME Hooks successfully!");
	for(const char *name : requirementNames)
		PinkCore::RequirementAPI::RegisterInstalled(name);
}
