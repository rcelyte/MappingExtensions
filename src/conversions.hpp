#pragma once
#include <BeatmapSaveDataVersion3/BeatmapSaveData_BpmChangeEventData.hpp>

template<class SaveData> struct Linematch;

struct BpmState {
	struct BpmChangeData {
		float time, beat, bpm;
	};
	std::vector<BpmChangeData> changes;
	uint32_t current = 0;
	BpmState(float startBpm, System::Collections::Generic::List_1<BeatmapSaveDataVersion3::BeatmapSaveData::BpmChangeEventData*> *events) {
		const uint32_t count = events->get_Count();
		bool skipFirst = (count > 0 && events->get_Item(0)->get_beat() == 0);
		changes.reserve(count + !skipFirst);
		BpmChangeData lastEntry = {0, 0, skipFirst ? events->get_Item(0)->get_bpm() : startBpm};
		changes.push_back(lastEntry);
		for(uint32_t i = skipFirst; i < count; i++) {
			BeatmapSaveDataVersion3::BeatmapSaveData::BpmChangeEventData *event = events->get_Item(i);
			const float beat = event->get_beat();
			lastEntry = {lastEntry.time + (beat - lastEntry.beat) / lastEntry.bpm * 60, beat, event->get_bpm()};
			changes.push_back(lastEntry);
		}
	}
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

template<> struct Linematch<BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData> {
	float time;
	int32_t lineIndex;
	GlobalNamespace::ColorType colorType;
	GlobalNamespace::NoteCutDirection cutDirection;
	float cutDirectionAngleOffset;
	Linematch(GlobalNamespace::NoteData *from) :
		time(from->get_time()),
		lineIndex(from->get_lineIndex()),
		colorType(from->get_colorType()),
		cutDirection(from->get_cutDirection()),
		cutDirectionAngleOffset(from->get_cutDirectionAngleOffset()) {}
	Linematch(BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData *from, BpmState *bpmState) :
		time(bpmState->GetTime(from->get_beat())),
		lineIndex(from->get_line()),
		colorType(ConvertColorType(from->get_color())),
		cutDirection(from->get_cutDirection()),
		cutDirectionAngleOffset(from->get_angleOffset()) {}
};

template<> struct Linematch<BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData> {
	float time;
	int32_t lineIndex;
	Linematch(GlobalNamespace::NoteData *from) :
		time(from->get_time()),
		lineIndex(from->get_lineIndex()) {}
	Linematch(BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData *from, BpmState *bpmState) :
		time(bpmState->GetTime(from->get_beat())),
		lineIndex(from->get_line()) {}
};


template<> struct Linematch<BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData> {
	float time;
	int32_t lineIndex;
	float duration;
	int32_t width;
	int32_t height;
	Linematch(GlobalNamespace::ObstacleData *from) :
		time(from->get_time()),
		lineIndex(from->get_lineIndex()),
		duration(from->get_duration()),
		width(from->get_width()),
		height(from->get_height()) {}
	Linematch(BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData *from, BpmState *bpmState) :
		time(bpmState->GetTime(from->get_beat())),
		lineIndex(from->get_line()),
		duration(bpmState->GetTime(from->get_beat() + from->get_duration()) - time),
		width(from->get_width()),
		height(from->get_height()) {}
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

template<> struct Linematch<BeatmapSaveDataVersion3::BeatmapSaveData::SliderData> {
	SliderLinematch base;
	GlobalNamespace::NoteCutDirection tailCutDirection;
	GlobalNamespace::SliderMidAnchorMode midAnchorMode;
	float headControlPointLengthMultiplier;
	float tailControlPointLengthMultiplier;
	Linematch(GlobalNamespace::SliderData *from) :
		base(from),
		tailCutDirection(from->get_tailCutDirection()),
		midAnchorMode(from->get_midAnchorMode()),
		headControlPointLengthMultiplier(from->get_headControlPointLengthMultiplier()),
		tailControlPointLengthMultiplier(from->get_tailControlPointLengthMultiplier()) {}
	Linematch(BeatmapSaveDataVersion3::BeatmapSaveData::SliderData *from, BpmState *bpmState) :
		base(from, bpmState),
		tailCutDirection(from->get_tailCutDirection()),
		midAnchorMode(from->get_sliderMidAnchorMode()),
		headControlPointLengthMultiplier(from->get_headControlPointLengthMultiplier()),
		tailControlPointLengthMultiplier(from->get_tailControlPointLengthMultiplier()) {}
};

template<> struct Linematch<BeatmapSaveDataVersion3::BeatmapSaveData::BurstSliderData> {
	SliderLinematch base;
	int32_t sliceCount;
	float squishAmount;
	Linematch(GlobalNamespace::SliderData *from) :
		base(from),
		sliceCount(from->get_sliceCount()),
		squishAmount(from->get_squishAmount()) {}
	Linematch(BeatmapSaveDataVersion3::BeatmapSaveData::BurstSliderData *from, BpmState *bpmState) :
		base(from, bpmState),
		sliceCount(from->get_sliceCount()),
		squishAmount(from->get_squishAmount()) {}
};

template<> struct Linematch<BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData> {
	float time;
	int32_t lineIndex;
	GlobalNamespace::OffsetDirection offsetDirection;
	Linematch(GlobalNamespace::WaypointData *from) :
		time(from->get_time()),
		lineIndex(from->get_lineIndex()),
		offsetDirection(from->get_offsetDirection()) {}
	Linematch(BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData *from, BpmState *bpmState) :
		time(bpmState->GetTime(from->get_beat())),
		lineIndex(from->get_line()),
		offsetDirection(from->get_offsetDirection()) {}
};

std::array<int32_t, 1> GetLineLayers(BeatmapSaveDataVersion3::BeatmapSaveData::BombNoteData *saveData) {return {saveData->get_layer()};}
std::array<int32_t, 1> GetLineLayers(BeatmapSaveDataVersion3::BeatmapSaveData::ColorNoteData *saveData) {return {saveData->get_layer()};}
std::array<int32_t, 1> GetLineLayers(BeatmapSaveDataVersion3::BeatmapSaveData::ObstacleData *saveData) {return {saveData->get_layer()};}
std::array<int32_t, 1> GetLineLayers(BeatmapSaveDataVersion3::BeatmapSaveData::WaypointData *saveData) {return {saveData->get_layer()};}
std::array<int32_t, 2> GetLineLayers(BeatmapSaveDataVersion3::BeatmapSaveData::BaseSliderData *saveData) {return {saveData->get_headLayer(), saveData->get_tailLayer()};}

template<class SaveData, std::size_t layerCount = 1>
struct LayerCache {
	struct Entry {
		Linematch<SaveData> match;
		std::array<int32_t, layerCount> layers;
	};
	std::vector<Entry> cache;
	std::vector<bool> matched;
	size_t head = 0, failCount = 0;
	LayerCache(System::Collections::Generic::List_1<SaveData*> *list, float startBpm, System::Collections::Generic::List_1<BeatmapSaveDataVersion3::BeatmapSaveData::BpmChangeEventData*> *bpmEvents) : matched(list->get_Count()) {
		BpmState bpmState(startBpm, bpmEvents);
		cache.reserve(matched.size());
		for(size_t i = 0; i < matched.size(); ++i) {
			SaveData *entry = list->get_Item(i);
			cache.push_back({Linematch<SaveData>(entry, &bpmState), GetLineLayers(entry)});
		}
	}
	template<class BeatmapData> std::array<int32_t, layerCount> restore(BeatmapData *data) {
		const Linematch<SaveData> match(data);
		std::array<int32_t, layerCount> res = {};
		size_t i = head;
		for(; i < cache.size(); ++i) {
			if(matched[i])
				continue;
			if(memcmp(&match, &cache[i].match, sizeof(match)))
				continue;
			res = cache[i].layers;
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
