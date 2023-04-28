#pragma once
#include <BeatmapSaveDataVersion3/BeatmapSaveData_BpmChangeEventData.hpp>

struct BpmChangeData {
	float time, beat, bpm;
};

static inline std::vector<BpmChangeData> ConvertBpmChanges(float startBpm, System::Collections::Generic::List_1<BeatmapSaveDataVersion3::BeatmapSaveData::BpmChangeEventData*> *events) {
	const uint32_t count = events->get_Count();
	bool skipFirst = (count > 0 && events->get_Item(0)->get_beat() == 0);
	std::vector<BpmChangeData> changes;
	changes.reserve(count + !skipFirst);
	BpmChangeData lastEntry = {0, 0, skipFirst ? events->get_Item(0)->get_bpm() : startBpm};
	changes.push_back(lastEntry);
	for(uint32_t i = skipFirst; i < count; i++) {
		BeatmapSaveDataVersion3::BeatmapSaveData::BpmChangeEventData *event = events->get_Item(i);
		const float beat = event->get_beat();
		lastEntry = {lastEntry.time + (beat - lastEntry.beat) / lastEntry.bpm * 60, beat, event->get_bpm()};
		changes.push_back(lastEntry);
	}
	return changes;
}

struct BpmState {
	std::basic_string_view<BpmChangeData> changes;
	uint32_t current = 0;
	BpmState(std::basic_string_view<BpmChangeData> changes) : changes(changes) {}
	float GetTime(float beat) {
		while(current > 0 && changes[current].beat >= beat)
			--current;
		while(current < changes.size() - 1 && changes[current + 1].beat < beat)
			++current;
		return changes[current].time + (beat - changes[current].beat) / changes[current].bpm * 60;
	}
};

static inline GlobalNamespace::ColorType ConvertColorType(BeatmapSaveDataVersion3::BeatmapSaveData::NoteColorType noteType) {
	switch(noteType) {
		case BeatmapSaveDataVersion3::BeatmapSaveData::NoteColorType::ColorA: return GlobalNamespace::ColorType::ColorA;
		case BeatmapSaveDataVersion3::BeatmapSaveData::NoteColorType::ColorB: return GlobalNamespace::ColorType::ColorB;
		default: return GlobalNamespace::ColorType::None;
	}
}

template<class SaveData> struct RestoreBlob;

template<> struct RestoreBlob<BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData> {
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
		Ident(BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()),
			colorType(ConvertColorType(from->get_color())),
			cutDirection(from->get_cutDirection()),
			cutDirectionAngleOffset(from->get_angleOffset()) {}
	} ident;
	int32_t layer;
	RestoreBlob(BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData *from, BpmState *bpmState) : ident(from, bpmState), layer(from->get_layer()) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		Ident(GlobalNamespace::NoteData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()) {}
		Ident(BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()) {}
	} ident;
	int32_t layer;
	RestoreBlob(BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData *from, BpmState *bpmState) : ident(from, bpmState), layer(from->get_layer()) {}
};


template<> struct RestoreBlob<BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData> {
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
		Ident(BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()),
			duration(bpmState->GetTime(from->get_beat() + from->get_duration()) - time),
			width(from->get_width()),
			height(from->get_height()) {}
	} ident;
	int32_t layer;
	RestoreBlob(BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData *from, BpmState *bpmState) : ident(from, bpmState), layer(from->get_layer()) {}
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
	SliderLinematch(BeatmapSaveDataVersion3::BeatmapSaveData::BaseSliderData *from, BpmState *bpmState) :
		colorType(ConvertColorType(from->get_colorType())),
		headTime(bpmState->GetTime(from->get_beat())),
		headLineIndex(from->get_headLine()),
		headCutDirection(from->get_headCutDirection()),
		tailTime(bpmState->GetTime(from->get_tailBeat())),
		tailLineIndex(from->get_tailLine()) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::BeatmapSaveData::SliderData> {
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
		Ident(BeatmapSaveDataVersion3::BeatmapSaveData::SliderData *from, BpmState *bpmState) :
			base(from, bpmState),
			tailCutDirection(from->get_tailCutDirection()),
			midAnchorMode(from->get_sliderMidAnchorMode()),
			headControlPointLengthMultiplier(from->get_headControlPointLengthMultiplier()),
			tailControlPointLengthMultiplier(from->get_tailControlPointLengthMultiplier()) {}
	} ident;
	std::array<int32_t, 2> layer;
	RestoreBlob(BeatmapSaveDataVersion3::BeatmapSaveData::SliderData *from, BpmState *bpmState) : ident(from, bpmState), layer({from->get_headLayer(), from->get_tailLayer()}) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::BeatmapSaveData::BurstSliderData> {
	struct Ident {
		SliderLinematch base;
		int32_t sliceCount;
		float squishAmount;
		Ident(GlobalNamespace::SliderData *from) :
			base(from),
			sliceCount(from->get_sliceCount()),
			squishAmount(from->get_squishAmount()) {}
		Ident(BeatmapSaveDataVersion3::BeatmapSaveData::BurstSliderData *from, BpmState *bpmState) :
			base(from, bpmState),
			sliceCount(from->get_sliceCount()),
			squishAmount(from->get_squishAmount()) {}
	} ident;
	std::array<int32_t, 2> layer;
	RestoreBlob(BeatmapSaveDataVersion3::BeatmapSaveData::BurstSliderData *from, BpmState *bpmState) : ident(from, bpmState), layer({from->get_headLayer(), from->get_tailLayer()}) {}
};

template<> struct RestoreBlob<BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData> {
	struct Ident {
		float time;
		int32_t lineIndex;
		GlobalNamespace::OffsetDirection offsetDirection;
		Ident(GlobalNamespace::WaypointData *from) :
			time(from->get_time()),
			lineIndex(from->get_lineIndex()),
			offsetDirection(from->get_offsetDirection()) {}
		Ident(BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData *from, BpmState *bpmState) :
			time(bpmState->GetTime(from->get_beat())),
			lineIndex(from->get_line()),
			offsetDirection(from->get_offsetDirection()) {}
	} ident;
	int32_t layer;
	RestoreBlob(BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData *from, BpmState *bpmState) : ident(from, bpmState), layer(from->get_layer()) {}
};

template<class SaveData>
struct LayerCache {
	std::vector<RestoreBlob<SaveData>> cache;
	std::vector<bool> matched;
	size_t head = 0, failCount = 0;
	LayerCache(System::Collections::Generic::List_1<SaveData*> *list, const std::vector<BpmChangeData> *changes) : matched(list->get_Count()) {
		BpmState bpmState(std::basic_string_view<BpmChangeData>(&(*changes)[0], changes->size()));
		cache.reserve(matched.size());
		for(size_t i = 0; i < matched.size(); ++i)
			cache.emplace_back(list->get_Item(i), &bpmState);
	}
	using LayerType = decltype(RestoreBlob<SaveData>::layer);
	template<class BeatmapData> LayerType restore(BeatmapData *data) {
		const typename RestoreBlob<SaveData>::Ident match(data);
		LayerType res = {};
		size_t i = head;
		for(; i < cache.size(); ++i) {
			if(matched[i])
				continue;
			if(memcmp(&match, &cache[i].ident, sizeof(match)))
				continue;
			res = cache[i].layer;
			matched[i] = true;
			break;
		}
		failCount += (i == cache.size());
		for(; head < cache.size(); ++head)
			if(!matched[head])
				break;
		return res;
	}
};
