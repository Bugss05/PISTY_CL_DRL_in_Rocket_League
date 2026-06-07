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

	Thread-safety: o Scheduler chama SetWeights()/SetStochastic() a partir do StepCallback,
	que corre depois de envSet->Sync() (step sincronizado) — não há ResetArena em curso nesse
	instante, logo mutar os pesos é seguro. O estado determinístico (`counts`) é por-instância
	(uma instância por arena) e cada arena só é resetada por uma thread de cada vez.
*/
class SchedulableState : public RLGC::StateSetter {
public:
	struct Entry {
		std::string name;          // label usado no SchedulerConfig para referenciar este setter
		RLGC::StateSetter* setter;
		float weight;
	};

	SchedulableState(const std::vector<Entry>& entries, bool stochastic = true)
		: entries(entries), stochastic(stochastic) {
		counts.resize(this->entries.size(), 0.0);
	}

	void SetStochastic(bool s) { stochastic = s; }
	bool IsStochastic() const { return stochastic; }

	// Atualização PARCIAL: só altera os setters cujo nome está no mapa.
	// Setters não listados mantêm o peso anterior.
	void SetWeights(const std::unordered_map<std::string, float>& weights) {
		for (auto& e : entries) {
			auto it = weights.find(e.name);
			if (it != weights.end())
				e.weight = it->second;
		}
	}

	void ResetArena(RocketSim::Arena* arena) override {
		int idx = stochastic ? PickStochastic() : PickDeterministic();
		if (idx >= 0)
			entries[idx].setter->ResetArena(arena);
	}

private:
	std::vector<Entry> entries;
	bool stochastic;

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
		return -1; // só por segurança numérica
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
			double target = (entries[i].weight / total) * totalResets;
			double deficit = target - counts[i];
			if (deficit > bestDeficit) {
				bestDeficit = deficit;
				best = i;
			}
		}
		if (best >= 0) counts[best] += 1.0;
		return best;
	}
};
