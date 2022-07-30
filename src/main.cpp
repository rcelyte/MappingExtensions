// https://github.com/rxzz0/MappingExtensions/blob/main/src/main.cpp
// Refactored and updated to 1.21.0 by rcelyte

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

#include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveData_ObstacleData.hpp>
#include <GlobalNamespace/BeatmapCharacteristicSO.hpp>
#include <GlobalNamespace/BeatmapData.hpp>
#include <GlobalNamespace/BeatmapDataLoader.hpp>
#include <GlobalNamespace/BeatmapDataLoader_ObstacleConvertor.hpp>
#include <GlobalNamespace/BeatmapDataObstaclesMergingTransform.hpp>
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
#include <System/Collections/Generic/List_1.hpp>
#include <System/Decimal.hpp>
#include <System/Single.hpp>
#include <UnityEngine/Camera.hpp>
#include <UnityEngine/Color.hpp>
#include <UnityEngine/Graphics.hpp>
#include <UnityEngine/Resources.hpp>
// #include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapObjectType.hpp>
// #include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveData_EventData.hpp>
// #include <BeatmapSaveDataVersion2_6_0AndEarlier/BeatmapSaveData_NoteData.hpp>
// #include <GlobalNamespace/SpawnRotationProcessor.hpp>
#include <sombrero/shared/FastVector2.hpp>
#include <sombrero/shared/FastVector3.hpp>
#include <sombrero/shared/FastQuaternion.hpp>
#include <pinkcore/shared/RequirementAPI.hpp>


using namespace GlobalNamespace;
using namespace System::Collections;


static ModInfo modInfo;

Logger& logger() {
	static Logger *logger = new Logger(modInfo, LoggerOptions(false, true));
	return *logger;
}

[[maybe_unused]] static void dump_real(int before, int after, void* ptr) {
	logger().info("Dumping Immediate Pointer: %p: %lx", ptr, *reinterpret_cast<long*>(ptr));
	long *begin = static_cast<long*>(ptr) - before;
	long *end   = static_cast<long*>(ptr) + after;
	for(long *cur = begin; cur != end; ++cur)
		logger().info("0x%lx: %lx", (long)cur - (long)ptr, *cur);
}

// Normalized indices are faster to compute & reverse, and more accurate than, effective indices (see below).
// A "normalized" precision index is an effective index * 1000. So unlike normal precision indices, only 0 is 0.
int ToNormalizedPrecisionIndex(int index) {
	if(index <= -1000)
		return index + 1000;
	if(index >= 1000)
		return index - 1000;
	return index * 1000;
}
int FromNormalizedPrecisionIndex(int index) {
	if(index % 1000 == 0) {
		return index / 1000;
	} else if(index > 0) {
		return index + 1000;
	} else {
		return index - 1000;
	}
}

// An effective index is a normal/extended index, but with decimal places that do what you'd expect.
float ToEffectiveIndex(int index) {
	float effectiveIndex = index;
	if(effectiveIndex <= -1000) {
		effectiveIndex = effectiveIndex / 1000.0f + 1.0f;
	} else if(effectiveIndex >= 1000) {
		effectiveIndex = effectiveIndex / 1000.0f - 1.0f;
	}
	return effectiveIndex;
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

/*static IDifficultyBeatmap* storedDiffBeatmap = nullptr;
static bool skipWallRatings = false;
MAKE_HOOK_MATCH(BeatmapObjectSpawnController_Start, &BeatmapObjectSpawnController::Start, void, BeatmapObjectSpawnController* self) {
	if(storedDiffBeatmap) {
		float njs = storedDiffBeatmap->get_noteJumpMovementSpeed();
		if(njs < 0)
			self->initData->noteJumpMovementSpeed = njs;
	}
	skipWallRatings = false;
	return BeatmapObjectSpawnController_Start(self);
}*/

/*MAKE_HOOK_MATCH(BeatmapObjectExecutionRatingsRecorder_HandleObstacleDidPassAvoidedMark, &BeatmapObjectExecutionRatingsRecorder::HandleObstacleDidPassAvoidedMark, void, BeatmapObjectExecutionRatingsRecorder* self, ObstacleController* obstacleController) {
	if(skipWallRatings)
		return;
	return BeatmapObjectExecutionRatingsRecorder_HandleObstacleDidPassAvoidedMark(self, obstacleController);
}*/

/* PC version hooks */

MAKE_HOOK_MATCH(BeatmapDataLoader_ConvertNoteLineLayer, &BeatmapDataLoader::ConvertNoteLineLayer, GlobalNamespace::NoteLineLayer, int layer) {
	GlobalNamespace::NoteLineLayer result = BeatmapDataLoader_ConvertNoteLineLayer(layer);
	if(layer > 2 || layer < 0)
		return layer;
	return result;
}

MAKE_HOOK_MATCH(ObstacleConvertor_GetNoteLineLayer, &BeatmapDataLoader::ObstacleConvertor::GetNoteLineLayer, GlobalNamespace::NoteLineLayer, int lineLayer) {
	GlobalNamespace::NoteLineLayer result = ObstacleConvertor_GetNoteLineLayer(lineLayer);
	if(lineLayer > 2)
		return lineLayer;
	return result;
}

static uint32_t currentNote = 0;
static std::vector<int32_t> lineIndexes; // TODO: should we worry about RAM here?

// TODO: pray this hooks correctly and isn't inlined
MAKE_HOOK_MATCH(BeatmapObjectsInTimeRowProcessor_SliderHeadPositionOverlapsWithNote, &BeatmapObjectsInTimeRowProcessor::SliderHeadPositionOverlapsWithNote, bool, ::GlobalNamespace::SliderData* slider, ::GlobalNamespace::NoteData* note) {
	if(currentNote < lineIndexes.size())
		note->set_lineIndex(lineIndexes[currentNote++]);
	return BeatmapObjectsInTimeRowProcessor_SliderHeadPositionOverlapsWithNote(slider, note);
}

static bool SliderHeadPositionOverlapsWithNote(SliderData *slider, NoteData *note) {
	return slider->headLineIndex == note->lineIndex && slider->headLineLayer == note->noteLineLayer;
}
static bool SliderTailPositionOverlapsWithNote(SliderData *slider, NoteData *note) {
	return slider->tailLineIndex == note->lineIndex && slider->tailLineLayer == note->noteLineLayer;
}
MAKE_HOOK_MATCH(BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, &BeatmapObjectsInTimeRowProcessor::HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice, void, BeatmapObjectsInTimeRowProcessor *self, ::GlobalNamespace::BeatmapObjectsInTimeRowProcessor::TimeSliceContainer_1<::GlobalNamespace::BeatmapDataItem*>* allObjectsTimeSlice, float nextTimeSliceTime) {
	lineIndexes.clear();
	currentNote = 0;
	System::Collections::Generic::IReadOnlyList_1<BeatmapDataItem*> *items = allObjectsTimeSlice->get_items();
	uint32_t itemCount = ((System::Collections::Generic::IReadOnlyCollection_1<BeatmapDataItem*>*)items)->get_Count();
	for(uint32_t i = 0; i < itemCount; ++i) {
		NoteData *note = il2cpp_utils::try_cast<NoteData>(items->get_Item(i)).value_or(nullptr);
		if(!note)
			continue;
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
	for(uint32_t i = 0; i < itemCount; ++i) {
		NoteData *note = il2cpp_utils::try_cast<NoteData>(items->get_Item(i)).value_or(nullptr);
		if(!note)
			continue;
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
	if(noteLineIndex == 4839)
		logger().info("lineIndex %i and lineLayer %i!", noteLineIndex, noteLineLayer.value);
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetNoteOffset(self, noteLineIndex, noteLineLayer);
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	num += noteLineIndex * (StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000);
	return Sombrero::FastVector3(self->rightVec) * num + Sombrero::FastVector3(0, StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer), 0);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_Get2DNoteOffset, &BeatmapObjectSpawnMovementData::Get2DNoteOffset, UnityEngine::Vector2, BeatmapObjectSpawnMovementData* self, int noteLineIndex, GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector2 result = BeatmapObjectSpawnMovementData_Get2DNoteOffset(self, noteLineIndex, noteLineLayer);
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	float x = num + noteLineIndex * (StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000);
	float y = StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer);
	return UnityEngine::Vector2(x, y);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_GetObstacleOffset, &BeatmapObjectSpawnMovementData::GetObstacleOffset, UnityEngine::Vector3, BeatmapObjectSpawnMovementData *self, int noteLineIndex, ::GlobalNamespace::NoteLineLayer noteLineLayer) {
	UnityEngine::Vector3 result = BeatmapObjectSpawnMovementData_GetObstacleOffset(self, noteLineIndex, noteLineLayer);
	if(noteLineIndex <= -1000)
		noteLineIndex += 2000;
	else if(noteLineIndex < 1000)
		return result;
	float num = -(self->noteLinesCount - 1) * .5f;
	num += noteLineIndex * (StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance / 1000);
	return Sombrero::FastVector3(self->rightVec) * num + Sombrero::FastVector3(0, StaticBeatmapObjectSpawnMovementData::LineYPosForLineLayer(noteLineLayer) + StaticBeatmapObjectSpawnMovementData::kObstacleVerticalOffset, 0);
}

MAKE_HOOK_MATCH(BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer, &BeatmapObjectSpawnMovementData::HighestJumpPosYForLineLayer, float, BeatmapObjectSpawnMovementData* self, NoteLineLayer lineLayer) {
	float result = BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer(self, lineLayer);
	float delta = (self->topLinesHighestJumpPosY - self->upperLinesHighestJumpPosY);
	if(lineLayer >= 1000 || lineLayer <= -1000)
		return self->upperLinesHighestJumpPosY - delta - delta + self->jumpOffsetYProvider->get_jumpOffsetY() + lineLayer * (delta / 1000);
	if(lineLayer > 2 || lineLayer < 0)
		return self->upperLinesHighestJumpPosY - delta + self->jumpOffsetYProvider->get_jumpOffsetY() + (lineLayer * delta);
	return result;
}

MAKE_HOOK_MATCH(BeatmapSaveData_SpawnRotationForEventValue, &BeatmapSaveDataVersion3::BeatmapSaveData::SpawnRotationForEventValue, float, int index) {
	float result = BeatmapSaveData_SpawnRotationForEventValue(index);
	if(storedBeatmapCharacteristicSO->requires360Movement && index >= 1000 && index <= 1720)
		return index - 1360;
	return result;
}

MAKE_HOOK_MATCH(BeatmapSaveData_GetHeightForObstacleType, &BeatmapSaveDataVersion3::BeatmapSaveData::GetHeightForObstacleType, int, ::BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::ObstacleType obstacleType) {
	int result = BeatmapSaveData_GetHeightForObstacleType(obstacleType);
	if((obstacleType < 1000 || obstacleType > 4000) && (obstacleType < 4001 || obstacleType > 4005000))
		return result;
	int32_t obsHeight;
	if(obstacleType >= 4001 && obstacleType <= 4100000)
		obsHeight = (obstacleType - 4001) / 1000;
	else
		obsHeight = obstacleType - 1000;
	float height = obsHeight / 1000.f * 5;
	return height * 1000 + 1000;
}

MAKE_HOOK_MATCH(BeatmapSaveData_GetLayerForObstacleType, &BeatmapSaveDataVersion3::BeatmapSaveData::GetLayerForObstacleType, int, ::BeatmapSaveDataVersion2_6_0AndEarlier::BeatmapSaveData::ObstacleType obstacleType) {
	int result = BeatmapSaveData_GetLayerForObstacleType(obstacleType);
	if((obstacleType < 1000 || obstacleType > 4000) && (obstacleType < 4001 || obstacleType > 4005000))
		return result;
	int startHeight = 0;
	if(obstacleType >= 4001 && obstacleType <= 4100000)
		startHeight = (obstacleType - 4001) % 1000;
	float layer = startHeight / 750.f * 5;
	return layer * 1000 + 1334;
}

MAKE_HOOK_MATCH(NoteBasicCutInfoHelper_GetBasicCutInfo, &NoteBasicCutInfoHelper::GetBasicCutInfo, void, ::UnityEngine::Transform* noteTransform, ::GlobalNamespace::ColorType colorType, ::GlobalNamespace::NoteCutDirection cutDirection, ::GlobalNamespace::SaberType saberType, float saberBladeSpeed, ::UnityEngine::Vector3 cutDirVec, float cutAngleTolerance, ByRef<bool> directionOK, ByRef<bool> speedOK, ByRef<bool> saberTypeOK, ByRef<float> cutDirDeviation, ByRef<float> cutDirAngle) {
	if(cutDirection >= 2000 && cutDirection <= 2360)
		cutDirection = NoteCutDirection::Any;
	NoteBasicCutInfoHelper_GetBasicCutInfo(noteTransform, colorType, cutDirection, saberType, saberBladeSpeed, cutDirVec, cutAngleTolerance, directionOK, speedOK, saberTypeOK, cutDirDeviation, cutDirAngle);
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Rotation, &NoteCutDirectionExtensions::Rotation, UnityEngine::Quaternion, NoteCutDirection cutDirection, float offset) {
	UnityEngine::Quaternion result = NoteCutDirectionExtensions_Rotation(cutDirection, offset);
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
	if(cutDirection >= 1000 && cutDirection <= 1360)
		return 1000 - cutDirection;
	if(cutDirection >= 2000 && cutDirection <= 2360)
		return 2000 - cutDirection;
	return result;
}

MAKE_HOOK_MATCH(NoteCutDirectionExtensions_Mirrored, &NoteCutDirectionExtensions::Mirrored, NoteCutDirection, NoteCutDirection cutDirection) {
	NoteCutDirection result = NoteCutDirectionExtensions_Mirrored(cutDirection);
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
	if(MirrorPrecisionLineIndex(&lineIndex))
		self->set_lineIndex(lineIndex);
	if(MirrorPrecisionLineIndex(&flipLineIndex))
		self->set_flipLineIndex(flipLineIndex);
}

MAKE_HOOK_MATCH(NoteJump_Init, &NoteJump::Init, void, NoteJump *self, float beatTime, float worldRotation, ::UnityEngine::Vector3 startPos, ::UnityEngine::Vector3 endPos, float jumpDuration, float gravity, float flipYSide, float endRotation, bool rotateTowardsPlayer, bool useRandomRotation) {
	if(endPos.x + endPos.y > .0001)
		return NoteJump_Init(self, beatTime, worldRotation, startPos, endPos, jumpDuration, gravity, flipYSide, endRotation, rotateTowardsPlayer, useRandomRotation);
	UnityEngine::Vector3 safeEnd = UnityEngine::Vector3(abs(endPos.x), abs(endPos.y), endPos.z);
	NoteJump_Init(self, beatTime, worldRotation, startPos, safeEnd, jumpDuration, gravity, flipYSide, endRotation, rotateTowardsPlayer, useRandomRotation);
	self->endPos = endPos;
	self->moveVec = (endPos - startPos) / jumpDuration;
}

// idk what this was all the old stuff was for, but here's the ported transpiler -rcelyte
MAKE_HOOK_MATCH(ObstacleController_Init, &ObstacleController::Init, void, ObstacleController* self, ObstacleData* obstacleData, float worldRotation, UnityEngine::Vector3 startPos, UnityEngine::Vector3 midPos, UnityEngine::Vector3 endPos, float move1Duration, float move2Duration, float singleLineWidth, float height) {
	if(height <= -1000)
		height = (obstacleData->height + 2000) / 1000.f * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(height >= 1000)
		height = (obstacleData->height - 1000) / 1000.f * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;
	else if(height > 2)
		height = obstacleData->height * StaticBeatmapObjectSpawnMovementData::kNoteLinesDistance;

	int32_t oldWidth = obstacleData->width;
	if(oldWidth <= -1000)
		obstacleData->width = oldWidth + 1000;
	else if(oldWidth >= 1000)
		obstacleData->width = oldWidth - 1000;
	else
		return ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth, height);
	float fix = singleLineWidth * (.5f - .5f / 1000);
	midPos.x += fix;
	endPos.x += fix;
	ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth / 1000, height);
	self->startPos.x += fix;
	obstacleData->width = oldWidth;

	/*ObstacleController_Init(self, obstacleData, worldRotation, startPos, midPos, endPos, move1Duration, move2Duration, singleLineWidth, height);
	if((obstacleData->get_obstacleType().value < 1000) && !(obstacleData->get_width() >= 1000))
		return;
	// Either wall height or wall width are precision

	skipWallRatings = true;
	int mode        = (obstacleData->get_obstacleType().value >= 4001 && obstacleData->get_obstacleType().value <= 4100000) ? 1 : 0;
	int obsHeight;
	int startHeight = 0;
	if(mode == 1) {
		int value = obstacleData->get_obstacleType().value;
		value -= 4001;
		obsHeight = value / 1000;
		startHeight = value % 1000;
	} else {
		int value = obstacleData->get_obstacleType().value;
		obsHeight = value - 1000; // won't be used unless height is precision
	}

	float num = (float)obstacleData->get_width() * singleLineWidth;
	if((obstacleData->get_width() >= 1000) || (mode == 1)) {
		if(obstacleData->get_width() >= 1000) {
			float width              = (float)obstacleData->get_width() - 1000.0f;
			float precisionLineWidth = singleLineWidth / 1000.0f;
			num                      = width * precisionLineWidth;
		}
		// Change y of b for start height
		UnityEngine::Vector3 b { b.x = (num - singleLineWidth) * 0.5f, b.y = 4 * ((float)startHeight / 1000), b.z = 0 };

		self->startPos = startPos + b;
		self->midPos   = midPos + b;
		self->endPos   = endPos + b;
	}

	float num2       = UnityEngine::Vector3::Distance(self->endPos, self->midPos) / move2Duration;
	float length     = num2 * obstacleData->get_duration();
	float multiplier = 1;
	if(obstacleData->get_obstacleType().value >= 1000) {
		multiplier = (float)obsHeight / 1000;
	}

	self->stretchableObstacle->SetSizeAndColor((num * 0.98f), (height * multiplier), length, self->get_color());
	self->bounds = self->stretchableObstacle->bounds;*/
}

MAKE_HOOK_MATCH(ObstacleData_Mirror, &ObstacleData::Mirror, void, ObstacleData* self, int lineCount) {
	int32_t lineIndex = self->lineIndex;
	ObstacleData_Mirror(self, lineCount);
	if(lineIndex >= 1000 || lineIndex <= -1000 || self->width >= 1000 || self->width <= -1000) {
		int32_t newIndex = (ToNormalizedPrecisionIndex(lineIndex) - 2000) * -1 + 2000;
		int32_t newWidth = ToNormalizedPrecisionIndex(self->width);
		newIndex -= newWidth;
		self->lineIndex = (newIndex < 0) ? newIndex - 1000 : newIndex + 1000;
	} else if(lineIndex < 0 || lineIndex > 3) {
		int32_t mirrorLane = (lineIndex - 2) * -1 + 2;
		self->lineIndex = mirrorLane - self->width;
	}
}

MAKE_HOOK_MATCH(SliderData_Mirror, &SliderData::Mirror, void, SliderData *self, int lineCount) {
	int32_t headLineIndex = self->headLineIndex;
	int32_t tailLineIndex = self->tailLineIndex;
	SliderData_Mirror(self, lineCount);
	if(MirrorPrecisionLineIndex(&headLineIndex))
		self->headLineIndex = headLineIndex;
	if(MirrorPrecisionLineIndex(&tailLineIndex))
		self->tailLineIndex = tailLineIndex;
}

MAKE_HOOK_MATCH(SliderMeshController_CutDirectionToControlPointPosition, &SliderMeshController::CutDirectionToControlPointPosition, UnityEngine::Vector3, NoteCutDirection noteCutDirection) {
	UnityEngine::Vector3 result = SliderMeshController_CutDirectionToControlPointPosition(noteCutDirection);
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
	constexpr float delta = StaticBeatmapObjectSpawnMovementData::kTopLinesYPos - StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos;
	if(lineLayer >= 1000 || lineLayer <= -1000)
		return StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta - delta + lineLayer * (delta / 1000);
	if(lineLayer > 2 || lineLayer < 0)
		return StaticBeatmapObjectSpawnMovementData::kUpperLinesYPos - delta + lineLayer * delta;
	return result;
}

/*MAKE_HOOK_MATCH(FlyingScoreSpawner_SpawnFlyingScore, &FlyingScoreSpawner::SpawnFlyingScore, void, FlyingScoreSpawner* self, ByRef<GlobalNamespace::NoteCutInfo> noteCutInfo, int noteLineIndex, int multiplier, UnityEngine::Vector3 pos, UnityEngine::Quaternion rotation, UnityEngine::Quaternion inverseRotation, UnityEngine::Color color) {
	if(noteLineIndex < 0)
		noteLineIndex = 0;
	if(noteLineIndex > 3)
		noteLineIndex = 3;
	return FlyingScoreSpawner_SpawnFlyingScore(self, noteCutInfo, noteLineIndex, multiplier, pos, rotation, inverseRotation, color);
}*/

/*MAKE_HOOK_MATCH(NoteData_MirrorTransformCutDirection, &NoteData::Mirror, void, NoteData* self, int lineCount) {
	int state = self->get_cutDirection().value;
	NoteData_MirrorTransformCutDirection(self, lineCount);
	if(state >= 1000) {
		int newdir         = 2360 - state;
		self->set_cutDirection(newdir);
	}
}*/

/* End of PC version hooks */

/*MAKE_HOOK_MATCH(BeatmapDataObstaclesMergingTransform_CreateTransformedData, &BeatmapDataObstaclesMergingTransform::CreateTransformedData, IReadonlyBeatmapData *, IReadonlyBeatmapData *beatmapData) {
	return beatmapData;
}*/

extern "C" void setup(ModInfo& info) {
	info.id = "MappingExtensions";
	info.version = "0.21.0";
	modInfo = info;
	logger().info("Leaving setup!");
}

extern "C" void load() {
	logger().info("Installing ME Hooks, please wait");
	il2cpp_functions::Init();

	Logger& hookLogger = logger();

	INSTALL_HOOK(hookLogger, StandardLevelDetailView_RefreshContent);
	INSTALL_HOOK(hookLogger, MainMenuViewController_DidActivate);
	// INSTALL_HOOK(hookLogger, BeatmapObjectSpawnController_Start);
	// INSTALL_HOOK(hookLogger, BeatmapObjectExecutionRatingsRecorder_HandleObstacleDidPassAvoidedMark);

	INSTALL_HOOK(hookLogger, BeatmapDataLoader_ConvertNoteLineLayer);
	INSTALL_HOOK(hookLogger, ObstacleConvertor_GetNoteLineLayer);
	INSTALL_HOOK(hookLogger, BeatmapObjectsInTimeRowProcessor_SliderHeadPositionOverlapsWithNote);
	INSTALL_HOOK(hookLogger, BeatmapObjectsInTimeRowProcessor_HandleCurrentTimeSliceAllNotesAndSlidersDidFinishTimeSlice);
	INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_GetNoteOffset);
	INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_Get2DNoteOffset);
	INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_GetObstacleOffset);
	INSTALL_HOOK(hookLogger, BeatmapObjectSpawnMovementData_HighestJumpPosYForLineLayer);
	INSTALL_HOOK(hookLogger, BeatmapSaveData_SpawnRotationForEventValue);
	INSTALL_HOOK(hookLogger, BeatmapSaveData_GetHeightForObstacleType);
	INSTALL_HOOK(hookLogger, BeatmapSaveData_GetLayerForObstacleType);
	// [HarmonyPatch(typeof(ColorNoteVisuals), nameof(ColorNoteVisuals.HandleNoteControllerDidInit))]
	INSTALL_HOOK(hookLogger, NoteBasicCutInfoHelper_GetBasicCutInfo);
	INSTALL_HOOK(hookLogger, NoteCutDirectionExtensions_Rotation);
	INSTALL_HOOK(hookLogger, NoteCutDirectionExtensions_Direction);
	INSTALL_HOOK(hookLogger, NoteCutDirectionExtensions_RotationAngle);
	INSTALL_HOOK(hookLogger, NoteCutDirectionExtensions_Mirrored);
	INSTALL_HOOK(hookLogger, NoteData_Mirror);
	INSTALL_HOOK(hookLogger, NoteJump_Init); // Fixed in 1.22.1
	INSTALL_HOOK(hookLogger, ObstacleController_Init);
	INSTALL_HOOK(hookLogger, ObstacleData_Mirror);
	INSTALL_HOOK(hookLogger, SliderData_Mirror);
	INSTALL_HOOK(hookLogger, SliderMeshController_CutDirectionToControlPointPosition);
	INSTALL_HOOK(hookLogger, StaticBeatmapObjectSpawnMovementData_LineYPosForLineLayer);
	// INSTALL_HOOK(hookLogger, FlyingScoreSpawner_SpawnFlyingScore);
	// INSTALL_HOOK(hookLogger, NoteData_MirrorTransformCutDirection);

	// INSTALL_HOOK(hookLogger, BeatmapDataObstaclesMergingTransform_CreateTransformedData);

	logger().info("Installed ME Hooks successfully!");
	PinkCore::RequirementAPI::RegisterInstalled("Mapping Extensions");    
}
