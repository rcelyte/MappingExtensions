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
#include <GlobalNamespace/BeatmapDataObstaclesMergingTransform.hpp>
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

using namespace GlobalNamespace;
using namespace System::Collections;


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

static bool active = false;
static void CheckRequirements(const std::vector<std::string> &requirements) {
	logger->info("CheckRequirements()");
	for(std::string req : requirements)
		logger->info("    %s", req.c_str());
	active = std::any_of(requirements.begin(), requirements.end(), [](const std::string &req) {
		return std::any_of(requirementNames.begin(), requirementNames.end(), [req](const char *name) {
			return req == name;
		});
	});
}

static BeatmapCharacteristicSO* storedBeatmapCharacteristicSO = nullptr;
MAKE_HOOK_MATCH(StandardLevelDetailView_RefreshContent, &StandardLevelDetailView::RefreshContent, void, StandardLevelDetailView* self) {
	StandardLevelDetailView_RefreshContent(self);
	storedBeatmapCharacteristicSO = self->get_selectedDifficultyBeatmap()->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic();
}
MAKE_HOOK_MATCH(MainMenuViewController_DidActivate, &MainMenuViewController::DidActivate, void, MainMenuViewController* self, bool firstActivation, bool addedToHierarchy, bool screenSystemEnabling) {
	storedBeatmapCharacteristicSO = nullptr;
	return MainMenuViewController_DidActivate(self, firstActivation, addedToHierarchy, screenSystemEnabling);
}

/* PC version hooks */

MAKE_HOOK_MATCH(BeatmapDataLoader_GetBeatmapDataFromBeatmapSaveData, &BeatmapDataLoader::GetBeatmapDataFromBeatmapSaveData, BeatmapData*, ::BeatmapSaveDataVersion3::BeatmapSaveData* beatmapSaveData, ::GlobalNamespace::BeatmapDifficulty beatmapDifficulty, float startBpm, bool loadingForDesignatedEnvironment, ::GlobalNamespace::EnvironmentKeywords* environmentKeywords, ::GlobalNamespace::EnvironmentLightGroups* environmentLightGroups, ::GlobalNamespace::DefaultEnvironmentEvents* defaultEnvironmentEvents, ::GlobalNamespace::PlayerSpecificSettings* playerSpecificSettings) {
	System::Collections::Generic::List_1<::BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData*> *notes = beatmapSaveData->colorNotes;
	System::Collections::Generic::List_1<::BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData*> *bombs = beatmapSaveData->bombNotes;
	System::Collections::Generic::List_1<BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData*> *obstacles = beatmapSaveData->obstacles;
	System::Collections::Generic::List_1<::BeatmapSaveDataVersion3::BeatmapSaveData::SliderData*> *sliders = beatmapSaveData->sliders;
	System::Collections::Generic::List_1<::BeatmapSaveDataVersion3::BeatmapSaveData::BurstSliderData*> *bursts = beatmapSaveData->burstSliders;
	System::Collections::Generic::List_1<BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData*> *waypoints = beatmapSaveData->waypoints;

	uint32_t noteIndex = 0, bombIndex = 0, obstacleIndex = 0, sliderIndex = 0, burstIndex = 0, waypointIndex = 0;

	BeatmapData *result = BeatmapDataLoader_GetBeatmapDataFromBeatmapSaveData(beatmapSaveData, beatmapDifficulty, startBpm, loadingForDesignatedEnvironment, environmentKeywords, environmentLightGroups, defaultEnvironmentEvents, playerSpecificSettings);
	logger->info("Restoring %u notes, %u bombs, %u obstacles, %u sliders, %u burst sliders, and %u waypoints", notes->get_Count(), bombs->get_Count(), obstacles->get_Count(), sliders->get_Count(), bursts->get_Count(), waypoints->get_Count());
	for(System::Collections::Generic::LinkedListNode_1<BeatmapDataItem*> *iter = result->get_allBeatmapDataItems()->head, *end = iter ? iter->prev : NULL; iter; iter = iter->next) {
		BeatmapDataItem *item = iter->item;
		if(NoteData *data = il2cpp_utils::try_cast<NoteData>(item).value_or(nullptr); data) {
			System::Collections::Generic::IReadOnlyList_1<BeatmapSaveDataVersion3::BeatmapSaveData::BeatmapSaveDataItem*> *source = (System::Collections::Generic::IReadOnlyList_1<BeatmapSaveDataVersion3::BeatmapSaveData::BeatmapSaveDataItem*>*)notes;
			uint32_t *sourceIndex = &noteIndex;
			if(data->gameplayType == NoteData::GameplayType::Bomb)
				source = (System::Collections::Generic::IReadOnlyList_1<BeatmapSaveDataVersion3::BeatmapSaveData::BeatmapSaveDataItem*>*)bombs, sourceIndex = &bombIndex;
			if(*sourceIndex >= ((System::Collections::Generic::IReadOnlyCollection_1<BeatmapSaveDataVersion3::BeatmapSaveData::BeatmapSaveDataItem*>*)source)->get_Count()) {
				logger->warning("Failed to restore line layer for NoteData (%s)", ((void*)source == (void*)notes) ? "Color" : "Bomb");
				goto next;
			}
			BeatmapSaveDataVersion3::BeatmapSaveData::BeatmapSaveDataItem *saveNote = source->get_Item((*sourceIndex)++);
			// GlobalNamespace::NoteLineLayer oldLayer = data->noteLineLayer;
			if(BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData *saveData = il2cpp_utils::try_cast<BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData>(saveNote).value_or(nullptr); saveData)
				data->noteLineLayer = saveData->get_layer();
			else if(BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData *saveData = il2cpp_utils::try_cast<BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData>(saveNote).value_or(nullptr); saveData)
				data->noteLineLayer = saveData->get_layer();
			else
				logger->error("Failed to cast note data");
			/*if(data->noteLineLayer != oldLayer)
				logger->info("    NoteData restore %d -> %d", (int)oldLayer, (int)data->noteLineLayer);*/
		} else if(ObstacleData *data = il2cpp_utils::try_cast<ObstacleData>(item).value_or(nullptr); data) {
			if(obstacleIndex >= obstacles->get_Count()) {
				logger->warning("Failed to restore line layer for ObstacleData");
				goto next;
			}
			BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData *saveData = obstacles->get_Item(obstacleIndex++);
			if(!saveData) {
				logger->error("ObstacleData should not be null!");
				goto next;
			}
			// GlobalNamespace::NoteLineLayer oldLayer = data->lineLayer;
			data->lineLayer = saveData->get_layer();
			/*if(data->lineLayer != oldLayer)
				logger->info("    ObstacleData restore %d -> %d", (int)oldLayer, (int)data->lineLayer);*/
		} else if(SliderData *data = il2cpp_utils::try_cast<SliderData>(item).value_or(nullptr); data) {
			System::Collections::Generic::IReadOnlyList_1<BeatmapSaveDataVersion3::BeatmapSaveData::BaseSliderData*> *source = (System::Collections::Generic::IReadOnlyList_1<BeatmapSaveDataVersion3::BeatmapSaveData::BaseSliderData*>*)sliders;
			uint32_t *sourceIndex = &sliderIndex;
			if(data->sliderType == SliderData::Type::Burst)
				source = (System::Collections::Generic::IReadOnlyList_1<BeatmapSaveDataVersion3::BeatmapSaveData::BaseSliderData*>*)bursts, sourceIndex = &burstIndex;
			if(sliderIndex >= ((System::Collections::Generic::IReadOnlyCollection_1<BeatmapSaveDataVersion3::BeatmapSaveData::BaseSliderData*>*)source)->get_Count()) {
				logger->warning("Failed to restore line layers for SliderData (%s)", ((void*)source == (void*)sliders) ? "Normal" : "Burst");
				goto next;
			}
			// GlobalNamespace::NoteLineLayer oldLayers[2] = {data->headLineLayer, data->tailLineLayer};
			BeatmapSaveDataVersion3::BeatmapSaveData::BaseSliderData *saveData = source->get_Item((*sourceIndex)++);
			data->headBeforeJumpLineLayer = data->headLineLayer = saveData->get_headLayer();
			data->tailBeforeJumpLineLayer = data->tailLineLayer = saveData->get_tailLayer();
			/*if(data->headLineLayer != oldLayers[0] || data->tailLineLayer != oldLayers[1])
				logger->info("    SliderData restore (%d, %d) -> (%d, %d)", (int)oldLayers[0], (int)oldLayers[1], (int)data->headLineLayer, (int)data->tailLineLayer);*/
		} else if(WaypointData *data = il2cpp_utils::try_cast<WaypointData>(item).value_or(nullptr); data) {
			if(waypointIndex >= waypoints->get_Count()) {
				logger->warning("Failed to restore line layer for WaypointData");
				goto next;
			}
			BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData *saveData = waypoints->get_Item(waypointIndex++);
			if(!saveData) {
				logger->error("WaypointData should not be null!");
				goto next;
			}
			// GlobalNamespace::NoteLineLayer oldLayer = data->lineLayer;
			data->lineLayer = saveData->get_layer();
			/*if(data->lineLayer != oldLayer)
				logger->info("    WaypointData restore %d -> %d", (int)oldLayer, (int)data->lineLayer);*/
		}
		next: // TODO: goto bad
		if(iter == end)
			break;
	}
	if(notes->get_Count() != noteIndex || bombs->get_Count() != bombIndex || obstacles->get_Count() != obstacleIndex || sliders->get_Count() != sliderIndex || bursts->get_Count() != burstIndex || waypoints->get_Count() != waypointIndex)
		logger->warning("Failed to restore %u notes, %u bombs, %u obstacles, %u sliders, %u burst sliders, and %u waypoints", notes->get_Count() - noteIndex, bombs->get_Count() - bombIndex, obstacles->get_Count() - obstacleIndex, sliders->get_Count() - sliderIndex, bursts->get_Count() - burstIndex, waypoints->get_Count() - waypointIndex);
	return result;
}

static std::vector<int32_t> lineIndexes; // TODO: should we worry about RAM here?

static inline bool SliderHeadPositionOverlapsWithNote(SliderData *slider, NoteData *note) {
	return slider->headLineIndex == note->lineIndex && slider->headLineLayer == note->noteLineLayer;
}
static inline bool SliderTailPositionOverlapsWithNote(SliderData *slider, NoteData *note) {
	return slider->tailLineIndex == note->lineIndex && slider->tailLineLayer == note->noteLineLayer;
}
MAKE_HOOK_MATCH(BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, &BeatmapObjectsInTimeRowProcessor::HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, void, BeatmapObjectsInTimeRowProcessor *self, ::GlobalNamespace::BeatmapObjectsInTimeRowProcessor::TimeSliceContainer_1<::GlobalNamespace::BeatmapDataItem*>* allObjectsTimeSlice, float nextTimeSliceTime) {
	/*if(!active)
		return BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice(self, allObjectsTimeSlice, nextTimeSliceTime);*/
	lineIndexes.clear();
	System::Collections::Generic::IReadOnlyList_1<BeatmapDataItem*> *items = allObjectsTimeSlice->get_items();
	uint32_t itemCount = ((System::Collections::Generic::IReadOnlyCollection_1<BeatmapDataItem*>*)items)->get_Count();
	for(uint32_t i = 0; i < itemCount; ++i) {
		NoteData *note = il2cpp_utils::try_cast<NoteData>(items->get_Item(i)).value_or(nullptr);
		if(!note)
			continue;
		// logger->info("CLAMP %u", i);
		lineIndexes.push_back(note->lineIndex);
		note->lineIndex = std::clamp(note->lineIndex, 0, 3);
	}
	BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice(self, allObjectsTimeSlice, nextTimeSliceTime);
	/*IEnumerable<NoteData> enumerable = allObjectsTimeSlice.OfType<NoteData>();
	IEnumerable<SliderData> enumerable2 = allObjectsTimeSlice.OfType<SliderData>();
	IEnumerable<BeatmapObjectsInTimeRowProcessor.SliderTailData> enumerable3 = allObjectsTimeSlice.OfType<BeatmapObjectsInTimeRowProcessor.SliderTailData>();
	if(!enumerable.Any(x => x.lineIndex > 3 || x.lineIndex < 0))
		return;*/
	std::unordered_map<int32_t, std::vector<NoteData*>> notesInColumnsReusableProcessingDictionaryOfLists;
	for(uint32_t i = 0, currentNote = 0; i < itemCount; ++i) {
		NoteData *note = il2cpp_utils::try_cast<NoteData>(items->get_Item(i)).value_or(nullptr);
		if(!note)
			continue;
		note->lineIndex = lineIndexes[currentNote++];
		std::vector<NoteData*> *list = &notesInColumnsReusableProcessingDictionaryOfLists.try_emplace(note->lineIndex).first->second;
		NoteLineLayer lineLayer = note->noteLineLayer;
		std::vector<NoteData*>::const_iterator pos = std::find_if(list->begin(), list->end(), [lineLayer](NoteData *e) {
			return e->noteLineLayer > lineLayer;
		});
		list->insert(pos, note);
	}
	for(std::pair<const int, std::vector<NoteData*>> &list : notesInColumnsReusableProcessingDictionaryOfLists)
		for(int i = 0; i < list.second.size(); ++i)
			list.second[i]->SetBeforeJumpNoteLineLayer(i);
	for(uint32_t i = 0; i < itemCount; ++i) {
		SliderData *slider = il2cpp_utils::try_cast<SliderData>(items->get_Item(i)).value_or(nullptr);
		if(!slider)
			continue;
		for(uint32_t j = 0; j < itemCount; ++j) {
			NoteData *note = il2cpp_utils::try_cast<NoteData>(items->get_Item(j)).value_or(nullptr);
			if(!note)
				continue;
			if(!SliderHeadPositionOverlapsWithNote(slider, note))
				continue;
			slider->SetHasHeadNote(true);
			slider->SetHeadBeforeJumpLineLayer(note->beforeJumpNoteLineLayer);
			if(slider->sliderType != SliderData::Type::Burst) {
				note->ChangeToSliderHead();
				continue;
			}
			note->ChangeToBurstSliderHead();
			if(note->cutDirection != slider->tailCutDirection)
				continue;
			UnityEngine::Vector2 line = StaticBeatmapObjectSpawnMovementData::Get2DNoteOffset(note->lineIndex, self->numberOfLines, note->noteLineLayer) - StaticBeatmapObjectSpawnMovementData::Get2DNoteOffset(slider->tailLineIndex, self->numberOfLines, slider->tailLineLayer);
			float num = Vector2Extensions::SignedAngleToLine(NoteCutDirectionExtensions::Direction(note->cutDirection), line);
			if(abs(num) > 40)
				continue;
			note->SetCutDirectionAngleOffset(num);
			slider->SetCutDirectionAngleOffset(num, num);
		}
	}
	for(uint32_t i = 0; i < itemCount; ++i) {
		BeatmapObjectsInTimeRowProcessor::SliderTailData *tailData = il2cpp_utils::try_cast<BeatmapObjectsInTimeRowProcessor::SliderTailData>(items->get_Item(i)).value_or(nullptr);
		if(!tailData)
			continue;
		SliderData *slider = tailData->slider;
		for(uint32_t j = 0; j < itemCount; ++j) {
			NoteData *note = il2cpp_utils::try_cast<NoteData>(items->get_Item(j)).value_or(nullptr);
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

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetNoteOffset, &BeatmapObjectSpawnMovementData::GetNoteOffset, UnityEngine::Vector3, BeatmapObjectSpawnMovementData* self, int noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetNoteOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	num += noteLineIndex * (StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f);
	return Sombrero::FastVector3(self->rightVec) * num + Sombrero::FastVector3(0, StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer), 0);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_Get2DNoteOffset, &BeatmapObjectSpawnMovementData::Get2DNoteOffset, UnityEngine::Vector2, BeatmapObjectSpawnMovementData* self, int noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector2 result = BeatmapObjectSpawnMovementData_Get2DNoteOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	float x = num + noteLineIndex * (StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f);
	float y = StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer);
	return UnityEngine::Vector2(x, y);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetObstacleOffset, &BeatmapObjectSpawnMovementData::GetObstacleOffset, UnityEngine::Vector3, BeatmapObjectSpawnMovementData *self, int noteLineIndex, ::GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetObstacleOffset(self, noteLineIndex, noteLineLayer);
	if(!active)
		return result;
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	num += noteLineIndex * (StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000.f);
	return Sombrero::FastVector3(self->rightVec) * num + Sombrero::FastVector3(0, StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer) + StaticBeatmapObjectSpawnMovementData::kObstacleVerticalOffset, 0);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer, &BeatmapObjectSpawnMovementData::HighestJumpPosYForLineLayer, float, BeatmapObjectSpawnMovementData* self, NoteLineLayer lineLayer) {
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
	if(storedBeatmapCharacteristicSO->requires360Movement && index >= 1000 && index <= 1720)
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

MAKE_HOOK_MATCH(NoteBasicCutInfoHelper_GetBasicCutInfo, &NoteBasicCutInfoHelper::GetBasicCutInfo, void, ::UnityEngine::Transform* noteTransform, ::GlobalNamespace::ColorType colorType, ::GlobalNamespace::NoteCutDirection cutDirection, ::GlobalNamespace::SaberType saberType, float saberBladeSpeed, ::UnityEngine::Vector3 cutDirVec, float cutAngleTolerance, ByRef<bool> directionOK, ByRef<bool> speedOK, ByRef<bool> saberTypeOK, ByRef<float> cutDirDeviation, ByRef<float> cutDirAngle) {
	if(active && cutDirection >= 2000 && cutDirection <= 2360)
		cutDirection = NoteCutDirection::Any;
	NoteBasicCutInfoHelper_GetBasicCutInfo(noteTransform, colorType, cutDirection, saberType, saberBladeSpeed, cutDirVec, cutAngleTolerance, directionOK, speedOK, saberTypeOK, cutDirDeviation, cutDirAngle);
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Rotation, &NoteCutDirectionExtensions::Rotation, UnityEngine::Quaternion, NoteCutDirection cutDirection, float offset) {
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

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Direction, &NoteCutDirectionExtensions::Direction, UnityEngine::Vector2, NoteCutDirection cutDirection) {
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

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_RotationAngle, &NoteCutDirectionExtensions::RotationAngle, float, NoteCutDirection cutDirection) {
	float result = NoteCutDirectionExtensions_RotationAngle(cutDirection);
	if(!active)
		return result;
	if(cutDirection >= 1000 && cutDirection <= 1360)
		return 1000 - cutDirection;
	if(cutDirection >= 2000 && cutDirection <= 2360)
		return 2000 - cutDirection;
	return result;
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Mirrored, &NoteCutDirectionExtensions::Mirrored, NoteCutDirection, NoteCutDirection cutDirection) {
	NoteCutDirection result = NoteCutDirectionExtensions_Mirrored(cutDirection);
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
		*lineIndex = ((*lineIndex < 4000) ? 5000 : 3000) - *lineIndex;
		return true;
	}
	if(*lineIndex > 3 || *lineIndex < 0) {
		*lineIndex = 3 - *lineIndex;
		return true;
	}
	return false;
}

MAKE_HOOK_MATCH(NoteData_Mirror, &NoteData::Mirror, void, NoteData* self, int lineCount) {
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

MAKE_HOOK_MATCH(ObstacleController_Init, &ObstacleController::Init, void, ObstacleController* self, ObstacleData* obstacleData, float worldRotation, UnityEngine::Vector3 startPos, UnityEngine::Vector3 midPos, UnityEngine::Vector3 endPos, float move1Duration, float move2Duration, float singleLineWidth, float height) {
	if(!active)
		return ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth, height);
	if(obstacleData->height <= -1000)
		height = (obstacleData->height + 2000) / 1000.f * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(obstacleData->height >= 1000)
		height = (obstacleData->height - 1000) / 1000.f * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(obstacleData->height > 2)
		height = obstacleData->height * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;

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

MAKE_HOOK_MATCH(ObstacleData_Mirror, &ObstacleData::Mirror, void, ObstacleData* self, int lineCount) {
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

MAKE_HOOK_MATCH(SliderData_Mirror, &SliderData::Mirror, void, SliderData *self, int lineCount) {
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

MAKE_HOOK_MATCH(SliderMeshController_CutDirectionToControlPointPosition, &SliderMeshController::CutDirectionToControlPointPosition, UnityEngine::Vector3, NoteCutDirection noteCutDirection) {
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

MAKE_HOOK_MATCH(StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer, &StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer, float, NoteLineLayer lineLayer) {
	float result = StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer(lineLayer);
	if(!active)
		return result;
	constexpr float delta = StaticBeatmapObjectSpawnMovementData::kTopLinesYPos - StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos;
	if(lineLayer >= 1000 || lineLayer <= -1000)
		return StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta - delta + lineLayer * (delta / 1000.f);
	if(lineLayer > 2 || lineLayer < 0)
		return StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta + lineLayer * delta;
	return result;
}

extern "C" DL_EXPORT void setup(ModInfo& info) {
	info.id = "MappingExtensions";
	info.version = "0.21.2";
	modInfo = info;
	logger = new Logger(modInfo, LoggerOptions(false, true));
	logger->info("Leaving setup!");
}

extern "C" DL_EXPORT void load() {
	logger->info("Installing ME Hooks, please wait");
	il2cpp_functions::Init();

	INSTALL_HOOK(*logger, StandardLevelDetailView_RefreshContent);
	INSTALL_HOOK(*logger, MainMenuViewController_DidActivate);

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
	PinkCore::API::GetFoundRequirementCallbackSafe() += CheckRequirements;
}
