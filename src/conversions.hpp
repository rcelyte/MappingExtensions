#pragma once
#include <GlobalNamespace/BeatmapTypeConverters.hpp>

static inline std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> ConvertBpmChanges(float startBpm, System::Collections::Generic::List_1<BeatmapSaveDataVersion2_6_0AndEarlier::EventData*> *const events) {
	uint32_t first = 0, count = 0;
	for(int32_t i = events->get_Count() - 1; i >= 0; --i) {
		if(events->get_Item(i)->get_type() != BeatmapSaveDataCommon::BeatmapEventType::BpmChange)
			continue;
		first = static_cast<uint32_t>(i);
		++count;
	}
	const bool skipFirst = (count > 0 && std::bit_cast<uint32_t>(events->get_Item(static_cast<int32_t>(first))->get_beat()) == 0);
	std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> changes;
	changes.reserve(count + !skipFirst);
	GlobalNamespace::BpmTimeProcessor::BpmChangeData lastEntry = {0, 0, skipFirst ? events->get_Item(static_cast<int32_t>(first))->get_time() : startBpm}; // This looks wrong, but it matches what Beat Games did
	changes.push_back(lastEntry);
	for(uint32_t i = first + skipFirst; i < count; i++) {
		BeatmapSaveDataVersion2_6_0AndEarlier::EventData *const event = events->get_Item(static_cast<int32_t>(i));
		if(event->get_type() != BeatmapSaveDataCommon::BeatmapEventType::BpmChange)
			continue;
		const float beat = event->get_time();
		lastEntry = {GlobalNamespace::BpmTimeProcessor::CalculateTime(lastEntry, beat), beat, event->get_floatValue()};
		changes.push_back(lastEntry);
	}
	return changes;
}

static inline std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> ConvertBpmChanges(float startBpm, System::Collections::Generic::List_1<BeatmapSaveDataVersion3::BpmChangeEventData*> *const events) {
	const uint32_t count = static_cast<uint32_t>(events->get_Count());
	const bool skipFirst = (count > 0 && std::bit_cast<uint32_t>(events->get_Item(0)->get_beat()) == 0);
	std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> changes;
	changes.reserve(count + !skipFirst);
	GlobalNamespace::BpmTimeProcessor::BpmChangeData lastEntry = {0, 0, skipFirst ? events->get_Item(0)->get_bpm() : startBpm};
	changes.push_back(lastEntry);
	for(uint32_t i = skipFirst; i < count; i++) {
		BeatmapSaveDataVersion3::BpmChangeEventData *const event = events->get_Item(static_cast<int32_t>(i));
		const float beat = event->get_beat();
		lastEntry = {GlobalNamespace::BpmTimeProcessor::CalculateTime(lastEntry, beat), beat, event->get_bpm()};
		changes.push_back(lastEntry);
	}
	return changes;
}

struct BpmState {
	std::span<const GlobalNamespace::BpmTimeProcessor::BpmChangeData> changes;
	uint32_t current = 0;
	BpmState(std::span<const GlobalNamespace::BpmTimeProcessor::BpmChangeData> changeData) : changes(changeData) {}
	float GetTime(float beat) {
		while(current > 0 && changes[current].bpmChangeStartBpmTime >= beat)
			--current;
		while(current < changes.size() - 1 && changes[current + 1].bpmChangeStartBpmTime < beat)
			++current;
		return changes[current].bpmChangeStartTime + (beat - changes[current].bpmChangeStartBpmTime) / changes[current].bpm * 60;
	}
};

template<class TObjectData> struct RestoreBlob;

template<> struct RestoreBlob<BeatmapSaveDataVersion2_6_0AndEarlier::NoteData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		GlobalNamespace::NoteData::GameplayType gameplayType;
		GlobalNamespace::ColorType colorType;
		GlobalNamespace::NoteCutDirection cutDirection;
		Ident(GlobalNamespace::NoteData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()),
			gameplayType(from->get_gameplayType()),
			colorType(from->get_colorType()),
			cutDirection(from->get_cutDirection()) {}
		Ident(BeatmapSaveDataVersion2_6_0AndEarlier::NoteData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_time())),
			lineIndex(from->get_lineIndex()),
			gameplayType((from->get_type() == BeatmapSaveDataVersion2_6_0AndEarlier::NoteType::Bomb) ?
				GlobalNamespace::NoteData::GameplayType::Bomb : GlobalNamespace::NoteData::GameplayType::Normal),
			colorType((from->get_type() == BeatmapSaveDataVersion2_6_0AndEarlier::NoteType::Bomb) ? GlobalNamespace::ColorType::None :
				(from->get_type() == BeatmapSaveDataVersion2_6_0AndEarlier::NoteType::NoteA) ? GlobalNamespace::ColorType::ColorA : GlobalNamespace::ColorType::ColorB),
			cutDirection((from->get_type() == BeatmapSaveDataVersion2_6_0AndEarlier::NoteType::Bomb) ? GlobalNamespace::NoteCutDirection::None :
				GlobalNamespace::BeatmapTypeConverters::ConvertNoteCutDirection(from->get_cutDirection())) {}
	} ident;
	struct {
		BeatmapSaveDataCommon::NoteLineLayer lineLayer;
		BeatmapSaveDataCommon::NoteCutDirection cutDirection;
	} value;
	RestoreBlob(BeatmapSaveDataVersion2_6_0AndEarlier::NoteData *from, BpmState *bpmState) : ident(from, bpmState),
		value{from->get_lineLayer(), from->get_cutDirection()} {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion2_6_0AndEarlier::ObstacleData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		float duration;
		int32_t width;
		Ident(GlobalNamespace::ObstacleData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()),
			duration(from->get_duration()),
			width(from->get_width()) {}
		Ident(BeatmapSaveDataVersion2_6_0AndEarlier::ObstacleData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_time())),
			lineIndex(from->get_lineIndex()),
			duration(bpmState->GetTime(from->get_time() + from->get_duration()) - this->time),
			width(from->get_width()) {}
	} ident;
	BeatmapSaveDataVersion2_6_0AndEarlier::ObstacleType value;
	RestoreBlob(BeatmapSaveDataVersion2_6_0AndEarlier::ObstacleData *from, BpmState *bpmState) : ident(from, bpmState), value(from->get_type()) {}
};

struct SliderLinematch {
	GlobalNamespace::ColorType colorType;
	float headTime;
	int32_t headLineIndex;
	GlobalNamespace::NoteCutDirection headCutDirection;
	float tailTime;
	int32_t tailLineIndex;
	SliderLinematch(GlobalNamespace::SliderData *from) :
		colorType(from->get_colorType()),
		headTime(from->get_time()),
		headLineIndex(from->get_headLineIndex()),
		headCutDirection(from->get_headCutDirection()),
		tailTime(from->get_tailTime()),
		tailLineIndex(from->get_tailLineIndex()) {}
	SliderLinematch(BeatmapSaveDataVersion2_6_0AndEarlier::SliderData *from, BpmState *bpmState) :
		colorType(GlobalNamespace::BeatmapTypeConverters::ConvertNoteColorType(from->get_colorType())),
		headTime(bpmState->GetTime(from->get_time())),
		headLineIndex(from->get_headLineIndex()),
		headCutDirection(GlobalNamespace::BeatmapTypeConverters::ConvertNoteCutDirection(from->get_headCutDirection())),
		tailTime(bpmState->GetTime(from->get_tailTime())),
		tailLineIndex(from->get_tailLineIndex()) {}
	SliderLinematch(BeatmapSaveDataVersion3::BaseSliderData *from, BpmState *bpmState) :
		colorType(GlobalNamespace::BeatmapTypeConverters::ConvertNoteColorType(from->get_colorType())),
		headTime(bpmState->GetTime(from->get_beat())),
		headLineIndex(from->get_headLine()),
		headCutDirection(GlobalNamespace::BeatmapTypeConverters::ConvertNoteCutDirection(from->get_headCutDirection())),
		tailTime(bpmState->GetTime(from->get_tailBeat())),
		tailLineIndex(from->get_tailLine()) {}
};

struct SliderRestoreLayers {
	BeatmapSaveDataCommon::NoteLineLayer headLayer, tailLayer;
};

template<> struct RestoreBlob<BeatmapSaveDataVersion2_6_0AndEarlier::SliderData> {
	struct Ident {
		SliderLinematch base;
		GlobalNamespace::NoteCutDirection tailCutDirection;
		GlobalNamespace::SliderMidAnchorMode midAnchorMode;
		float headControlPointLengthMultiplier;
		float tailControlPointLengthMultiplier;
		Ident(GlobalNamespace::SliderData *from) :
			base(from),
			tailCutDirection(from->get_tailCutDirection()),
			midAnchorMode(from->get_midAnchorMode()),
			headControlPointLengthMultiplier(from->get_headControlPointLengthMultiplier()),
			tailControlPointLengthMultiplier(from->get_tailControlPointLengthMultiplier()) {}
		Ident(BeatmapSaveDataVersion2_6_0AndEarlier::SliderData *from, BpmState *bpmState) :
			base(from, bpmState),
			tailCutDirection(GlobalNamespace::BeatmapTypeConverters::ConvertNoteCutDirection(from->get_tailCutDirection())),
			midAnchorMode(GlobalNamespace::BeatmapTypeConverters::ConvertSliderMidAnchorMode(from->get_sliderMidAnchorMode())),
			headControlPointLengthMultiplier(from->get_headControlPointLengthMultiplier()),
			tailControlPointLengthMultiplier(from->get_tailControlPointLengthMultiplier()) {}
	} ident;
	SliderRestoreLayers value;
	RestoreBlob(BeatmapSaveDataVersion2_6_0AndEarlier::SliderData *from, BpmState *bpmState) :
		ident(from, bpmState), value{from->get_headLineLayer(), from->get_tailLineLayer()} {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion2_6_0AndEarlier::WaypointData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		GlobalNamespace::OffsetDirection offsetDirection;
		Ident(GlobalNamespace::WaypointData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()),
			offsetDirection(from->get_offsetDirection()) {}
		Ident(BeatmapSaveDataVersion2_6_0AndEarlier::WaypointData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_time())),
			lineIndex(from->get_lineIndex()),
			offsetDirection(GlobalNamespace::BeatmapTypeConverters::ConvertOffsetDirection(from->get_offsetDirection())) {}
	} ident;
	BeatmapSaveDataCommon::NoteLineLayer value;
	RestoreBlob(BeatmapSaveDataVersion2_6_0AndEarlier::WaypointData *from, BpmState *bpmState) : ident(from, bpmState), value(from->get_lineLayer()) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion2_6_0AndEarlier::EventData> {
	struct Ident {
		float time;
		bool early;
		Ident(GlobalNamespace::SpawnRotationBeatmapEventData *from) :
			time(from->get_time()),
			early(from->get_executionOrder() < 0) {}
		Ident(BeatmapSaveDataVersion2_6_0AndEarlier::EventData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_time())),
			early(from->get_type() == BeatmapSaveDataCommon::BeatmapEventType::Event14) {}
	} ident;
	int32_t value;
	RestoreBlob(BeatmapSaveDataVersion2_6_0AndEarlier::EventData *from, BpmState *bpmState) : ident(from, bpmState), value(from->get_value()) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::ColorNoteData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		GlobalNamespace::ColorType colorType;
		GlobalNamespace::NoteCutDirection cutDirection;
		float cutDirectionAngleOffset;
		Ident(GlobalNamespace::NoteData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()),
			colorType(from->get_colorType()),
			cutDirection(from->get_cutDirection()),
			cutDirectionAngleOffset(from->get_cutDirectionAngleOffset()) {}
		Ident(BeatmapSaveDataVersion3::ColorNoteData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()),
			colorType(GlobalNamespace::BeatmapTypeConverters::ConvertNoteColorType(from->get_color())),
			cutDirection(GlobalNamespace::BeatmapTypeConverters::ConvertNoteCutDirection(from->get_cutDirection())),
			cutDirectionAngleOffset(static_cast<float>(from->get_angleOffset())) {}
	} ident;
	struct {
		BeatmapSaveDataCommon::NoteLineLayer layer;
		BeatmapSaveDataCommon::NoteCutDirection cutDirection;
	} value;
	RestoreBlob(BeatmapSaveDataVersion3::ColorNoteData *from, BpmState *bpmState) : ident(from, bpmState), value{from->get_layer(), from->get_cutDirection()} {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::BombNoteData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		Ident(GlobalNamespace::NoteData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()) {}
		Ident(BeatmapSaveDataVersion3::BombNoteData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()) {}
	} ident;
	int32_t value;
	RestoreBlob(BeatmapSaveDataVersion3::BombNoteData *from, BpmState *bpmState) : ident(from, bpmState), value(from->get_layer()) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::ObstacleData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		float duration;
		int32_t width;
		int32_t height;
		Ident(GlobalNamespace::ObstacleData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()),
			duration(from->get_duration()),
			width(from->get_width()),
			height(from->get_height()) {}
		Ident(BeatmapSaveDataVersion3::ObstacleData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()),
			duration(bpmState->GetTime(from->get_beat() + from->get_duration()) - this->time),
			width(from->get_width()),
			height(from->get_height()) {}
	} ident;
	int32_t value;
	RestoreBlob(BeatmapSaveDataVersion3::ObstacleData *from, BpmState *bpmState) : ident(from, bpmState), value(from->get_layer()) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::SliderData> {
	struct Ident {
		SliderLinematch base;
		GlobalNamespace::NoteCutDirection tailCutDirection;
		GlobalNamespace::SliderMidAnchorMode midAnchorMode;
		float headControlPointLengthMultiplier;
		float tailControlPointLengthMultiplier;
		Ident(GlobalNamespace::SliderData *from) :
			base(from),
			tailCutDirection(from->get_tailCutDirection()),
			midAnchorMode(from->get_midAnchorMode()),
			headControlPointLengthMultiplier(from->get_headControlPointLengthMultiplier()),
			tailControlPointLengthMultiplier(from->get_tailControlPointLengthMultiplier()) {}
		Ident(BeatmapSaveDataVersion3::SliderData *from, BpmState *bpmState) :
			base(from, bpmState),
			tailCutDirection(GlobalNamespace::BeatmapTypeConverters::ConvertNoteCutDirection(from->get_tailCutDirection())),
			midAnchorMode(GlobalNamespace::BeatmapTypeConverters::ConvertSliderMidAnchorMode(from->get_sliderMidAnchorMode())),
			headControlPointLengthMultiplier(from->get_headControlPointLengthMultiplier()),
			tailControlPointLengthMultiplier(from->get_tailControlPointLengthMultiplier()) {}
	} ident;
	SliderRestoreLayers value;
	RestoreBlob(BeatmapSaveDataVersion3::SliderData *from, BpmState *bpmState) :
		ident(from, bpmState), value{from->get_headLayer(), from->get_tailLayer()} {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::BurstSliderData> {
	struct Ident {
		SliderLinematch base;
		int32_t sliceCount;
		float squishAmount;
		Ident(GlobalNamespace::SliderData *from) :
			base(from),
			sliceCount(from->get_sliceCount()),
			squishAmount(from->get_squishAmount()) {}
		Ident(BeatmapSaveDataVersion3::BurstSliderData *from, BpmState *bpmState) :
			base(from, bpmState),
			sliceCount(from->get_sliceCount()),
			squishAmount(from->get_squishAmount()) {}
	} ident;
	SliderRestoreLayers value;
	RestoreBlob(BeatmapSaveDataVersion3::BurstSliderData *from, BpmState *bpmState) :
		ident(from, bpmState), value{from->get_headLayer(), from->get_tailLayer()} {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::WaypointData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		GlobalNamespace::OffsetDirection offsetDirection;
		Ident(GlobalNamespace::WaypointData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()),
			offsetDirection(from->get_offsetDirection()) {}
		Ident(BeatmapSaveDataVersion3::WaypointData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()),
			offsetDirection(GlobalNamespace::BeatmapTypeConverters::ConvertOffsetDirection(from->get_offsetDirection())) {}
	} ident;
	BeatmapSaveDataCommon::NoteLineLayer value;
	RestoreBlob(BeatmapSaveDataVersion3::WaypointData *from, BpmState *bpmState) : ident(from, bpmState), value(from->get_layer()) {}
};

template<class TObjectData>
struct LayerCache {
	std::vector<RestoreBlob<TObjectData>> cache;
	std::vector<bool> matched;
	size_t head = 0, failCount = 0;
	LayerCache(System::Collections::Generic::List_1<TObjectData*> *const list,
			const std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> *const changes) : matched(static_cast<uint32_t>(list->get_Count())) {
		BpmState bpmState(std::span<const GlobalNamespace::BpmTimeProcessor::BpmChangeData>(&(*changes)[0], changes->size()));
		cache.reserve(matched.size());
		for(uint32_t i = 0; i < static_cast<uint32_t>(matched.size()); ++i)
			cache.emplace_back(list->get_Item(static_cast<int32_t>(i)), &bpmState);
	}
	LayerCache(System::Collections::Generic::List_1<TObjectData*> *const list,
			const std::vector<GlobalNamespace::BpmTimeProcessor::BpmChangeData> *const changes, bool (*const filter)(TObjectData*)) : matched() {
		BpmState bpmState(std::span<const GlobalNamespace::BpmTimeProcessor::BpmChangeData>(&(*changes)[0], changes->size()));
		for(uint32_t i = 0, count = static_cast<uint32_t>(list->get_Count()); i < count; ++i) {
			if(TObjectData *const item = list->get_Item(static_cast<int32_t>(i)); filter(item)) {
				cache.emplace_back(item, &bpmState);
				matched.emplace_back(false);
			}
		}
	}
	std::optional<decltype(RestoreBlob<TObjectData>::value)> restore(auto *const data) {
		const typename RestoreBlob<TObjectData>::Ident match(data);
		for(size_t i = head; i < cache.size(); ++i) {
			if(matched[i] || memcmp(&match, &cache[i].ident, sizeof(match)) != 0)
				continue;
			matched[i] = true;
			if(i == head) {
				do {
					++head;
				} while(head < cache.size() && matched[head]);
			}
			return cache[i].value;
		}
		++failCount;
		return std::nullopt;
	}
};
LayerCache(void) -> LayerCache<void>;
