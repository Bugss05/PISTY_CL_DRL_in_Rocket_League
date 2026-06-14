#pragma once
#include <RLGymCPP/TerminalConditions/TerminalCondition.h>
#include <string>
#include <vector>
#include <unordered_map>

/*
	Terminal condition agendável em runtime.

	Substitui o vetor de TerminalCondition* quando queres que o Scheduler possa mudar,
	por fase, QUAIS condições terminais estão ativas.

	Uso em EnvCreateFunc:
	    SchedulableTerminal* terminalSet = new SchedulableTerminal({
	        { "NoTouch",   new NoTouchCondition(2),    true },
	        { "GoalScore", new GoalScoreCondition(),   true },
	        { "Timeout",   new TimeoutCondition(60.f), true },
	    });
	    result.terminalConditions = { terminalSet };

	No SchedulerConfig.h, por fase:
	    .terminalActive = {
	        { "NoTouch", false },  // desativa NoTouch nesta fase
	    }

	Condições inativas não disparam mas o Reset() é sempre chamado em todas,
	para que o estado interno (e.g. timeSinceTouch) seja limpo entre episódios.
*/
class SchedulableTerminal : public RLGC::TerminalCondition {
public:
	struct Entry {
		std::string name;
		RLGC::TerminalCondition* cond;
		bool active;
	};

	SchedulableTerminal(const std::vector<Entry>& entries) : entries(entries) {}

	bool IsConditionActive(const std::string& name) const {
		for (const auto& e : entries)
			if (e.name == name) return e.active;
		return false;
	}

	// Atualização parcial: só altera as condições cujo nome está no mapa.
	void SetActive(const std::unordered_map<std::string, bool>& active) {
		for (auto& e : entries) {
			auto it = active.find(e.name);
			if (it != active.end())
				e.active = it->second;
		}
	}

	// Atualização PARCIAL de parâmetros: para cada condição cujo nome está no mapa,
	// encaminha cada (param -> valor) para TerminalCondition::SetParam (ex.: Timeout "seconds").
	void SetParams(const std::unordered_map<std::string, std::unordered_map<std::string, float>>& params) {
		for (auto& e : entries) {
			auto it = params.find(e.name);
			if (it == params.end())
				continue;
			for (auto& [key, value] : it->second)
				e.cond->SetParam(key, value);
		}
	}

	void Reset(const RLGC::GameState& initialState) override {
		for (auto& e : entries)
			e.cond->Reset(initialState);
		lastTriggerIdx = -1;
	}

	bool IsTerminal(const RLGC::GameState& currentState) override {
		lastTriggerIdx = -1;
		for (int i = 0; i < (int)entries.size(); i++) {
			if (entries[i].active && entries[i].cond->IsTerminal(currentState)) {
				lastTriggerIdx = i;
				return true;
			}
		}
		return false;
	}

	bool IsTruncation() override {
		if (lastTriggerIdx >= 0)
			return entries[lastTriggerIdx].cond->IsTruncation();
		return false;
	}

private:
	std::vector<Entry> entries;
	int lastTriggerIdx = -1;
};
