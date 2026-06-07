#pragma once
#include <RLGymCPP/StateSetters/StateSetter.h>
#include <RLGymCPP/Math.h>
#include <string>
#include <vector>
#include <unordered_map>

/*
	State setter agendável em runtime.

	Substitui o CombinedState quando queres que o Scheduler possa mudar, por fase,
	QUE cenários são usados e com que peso. Os pesos da lib (CombinedState) são privados
	e a amostragem é sempre aleatória — esta classe expõe ambos.

	Modo de amostragem (flag `stochastic`, true por defeito):
	  - stochastic = true  -> escolha aleatória ponderada. Os pesos são PROBABILIDADES relativas.
	  - stochastic = false -> distribuição determinística por PERCENTAGEM. Ao longo dos resets,
	                          a fração de cada cenário converge exatamente para a sua percentagem
	                          (largest-remainder). Cobertura reprodutível de cada cenário.

	Tracking de win rate:
	  - ResetArena() conta o episódio que acabou (activeEntryIdx anterior).
	  - NotifyGoal() é chamado pelo Scheduler quando state.goalScored == true.
	  - GetAndResetStats() agrega e limpa os contadores para a iteração seguinte.

	Thread-safety: o Scheduler chama SetWeights()/SetStochastic() a partir do StepCallback,
	que corre depois de envSet->Sync() (step sincronizado) — não há ResetArena em curso nesse
	instante, logo mutar os pesos é seguro. O estado determinístico (`counts`) é por-instância
	(uma instância por arena) e cada arena só é resetada por uma thread de cada vez.
*/
class SchedulableState : public RLGC::StateSetter {
public:
	struct Entry {
		std::string name;
		RLGC::StateSetter* setter;
		float weight;
	};

	struct IterStats {
		std::vector<std::string> names;
		std::vector<float>       weights;   // pesos atuais (para relatório)
		std::vector<int>         episodes;  // episódios completos nesta iteração
		std::vector<int>         goals;     // golos marcados nesta iteração
	};

	SchedulableState(const std::vector<Entry>& entries, bool stochastic = true)
		: entries(entries), stochastic(stochastic) {
		int n = (int)this->entries.size();
		counts.resize(n, 0.0);
		episodesPerEntry.resize(n, 0);
		goalsPerEntry.resize(n, 0);
	}

	void SetStochastic(bool s) { stochastic = s; }
	bool IsStochastic() const  { return stochastic; }

	// Atualização PARCIAL: só altera os setters cujo nome está no mapa.
	void SetWeights(const std::unordered_map<std::string, float>& weights) {
		for (auto& e : entries) {
			auto it = weights.find(e.name);
			if (it != weights.end())
				e.weight = it->second;
		}
	}

	// Chamado pelo Scheduler quando state.goalScored == true para esta arena.
	void NotifyGoal() {
		if (activeEntryIdx >= 0)
			goalsPerEntry[activeEntryIdx]++;
	}

	// Chamado pelo Scheduler no início de cada iteração PPO para ler e limpar contadores.
	IterStats GetAndResetStats() {
		IterStats s;
		int n = (int)entries.size();
		s.names.resize(n); s.weights.resize(n);
		s.episodes.resize(n, 0); s.goals.resize(n, 0);
		for (int i = 0; i < n; i++) {
			s.names[i]    = entries[i].name;
			s.weights[i]  = entries[i].weight;
			s.episodes[i] = episodesPerEntry[i];
			s.goals[i]    = goalsPerEntry[i];
			episodesPerEntry[i] = 0;
			goalsPerEntry[i]    = 0;
		}
		return s;
	}

	void ResetArena(RocketSim::Arena* arena) override {
		// Conta o episódio que acabou (o que correu com activeEntryIdx)
		if (activeEntryIdx >= 0)
			episodesPerEntry[activeEntryIdx]++;

		int idx = stochastic ? PickStochastic() : PickDeterministic();
		activeEntryIdx = idx;
		if (idx >= 0)
			entries[idx].setter->ResetArena(arena);
	}

private:
	std::vector<Entry> entries;
	bool stochastic;
	int  activeEntryIdx = -1;

	std::vector<int> episodesPerEntry;
	std::vector<int> goalsPerEntry;

	// Estado do modo determinístico (por-instância / por-arena)
	std::vector<double> counts;
	double totalResets = 0.0;

	float TotalWeight() const {
		float t = 0;
		for (auto& e : entries)
			if (e.weight > 0) t += e.weight;
		return t;
	}

	int PickStochastic() {
		float total = TotalWeight();
		if (total <= 0) return -1;
		float f = RocketSim::Math::RandFloat(0, total);
		float cum = 0;
		for (int i = 0; i < (int)entries.size(); i++) {
			if (entries[i].weight <= 0) continue;
			cum += entries[i].weight;
			if (f <= cum) return i;
		}
		return -1;
	}

	// Escolhe o setter com maior défice (alvo acumulado - usados até agora).
	int PickDeterministic() {
		float total = TotalWeight();
		if (total <= 0) return -1;
		totalResets += 1.0;
		int best = -1;
		double bestDeficit = -1e30;
		for (int i = 0; i < (int)entries.size(); i++) {
			if (entries[i].weight <= 0) continue;
			double target  = (entries[i].weight / total) * totalResets;
			double deficit = target - counts[i];
			if (deficit > bestDeficit) { bestDeficit = deficit; best = i; }
		}
		if (best >= 0) counts[best] += 1.0;
		return best;
	}
};
