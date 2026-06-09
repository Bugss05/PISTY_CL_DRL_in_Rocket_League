// Teste standalone da lógica do scheduler (sem torch, sem GPU, sem Learner).
// Exercita EXATAMENTE as funções de SchedulerCore.h usadas em produção.
//
// Compilar/correr via CMake:   cmake --build build --config Release --target scheduler_test
//                              build/scheduler_test

#include "SchedulerCore.h"
#include <cstdio>
#include <cmath>

static int g_fails = 0;
static int g_checks = 0;

static void check(bool cond, const char* desc) {
	g_checks++;
	if (!cond) {
		g_fails++;
		printf("  [FALHA] %s\n", desc);
	} else {
		printf("  [ok]    %s\n", desc);
	}
}

static bool approx(float a, float b, float tol = 1e-9f) {
	return std::fabs(a - b) <= tol;
}

int main() {
	SchedulerConfig cfg = GetSchedulerConfig();
	int nPhases = (int)cfg.phases.size();
	printf("Currículo com %d fases, interpolatePPOParams=%d\n\n",
		nPhases, (int)cfg.interpolatePPOParams);

	check(nPhases >= 1, "currículo tem pelo menos 1 fase");

	// ---- 1) Forward-fill dos optionals (SchedulerResolvePhases) ----
	printf("[1] ResolvePhases (forward-fill)\n");

	// Baseline com sentinelas para verificar herança de campos vazios ({}).
	ResolvedPPO baseline{};
	baseline.policyLR          = 9.0f;
	baseline.criticLR          = 9.0f;
	baseline.entropyScale      = 9.0f;
	baseline.clipRange         = 0.111f;
	baseline.policyTemperature = 0.222f;
	baseline.epochs            = 99;
	baseline.gaeGamma          = 9.0f;
	baseline.gaeLambda         = 0.333f;
	baseline.rewardClipRange   = 0.444f;

	std::vector<ResolvedPPO> r = SchedulerResolvePhases(cfg, baseline);

	check((int)r.size() == nPhases, "resolve retorna uma entrada por fase");

	// Fase 0 define policyLR=2e-4 explicitamente
	check(approx(r[0].policyLR, 2e-4f), "fase0.policyLR = 2e-4 (definido)");

	// Campos não definidos em nenhuma fase herdam o baseline
	check(approx(r[0].clipRange, 0.111f), "fase0.clipRange vazio -> baseline (0.111)");
	check(approx(r[0].policyTemperature, 0.222f), "fase0.policyTemperature -> baseline (0.222)");
	check(approx(r[0].gaeLambda, 0.333f), "fase0.gaeLambda -> baseline (0.333)");
	check(approx(r[0].rewardClipRange, 0.444f), "fase0.rewardClipRange -> baseline (0.444)");

	// Forward-fill: campo vazio herda o valor da fase anterior (não o baseline)
	if (nPhases >= 2) {
		// fase1 não define policyLR -> herda fase0 (2e-4), não baseline (9)
		check(approx(r[1].policyLR, r[0].policyLR), "fase1.policyLR vazio -> herda fase0");
		// campos sem definição em nenhuma fase mantêm o baseline em todas as fases
		check(approx(r[1].clipRange, 0.111f), "fase1.clipRange vazio -> baseline (forward-filled)");
	}

	// A última fase com policyLR definido deve ter o valor correto
	for (int i = 0; i < nPhases; i++) {
		if (cfg.phases[i].policyLR.has_value()) {
			check(approx(r[i].policyLR, *cfg.phases[i].policyLR),
				"fase com policyLR definido tem o valor correto no resolved");
		}
	}

	// ---- 2) SchedulerEffective em modo degrau (interpolatePPOParams = false) ----
	printf("\n[2] SchedulerEffective (modo degrau)\n");
	{
		SchedulerConfig step = cfg;
		step.interpolatePPOParams = false;

		for (int i = 0; i < nPhases; i++) {
			ResolvedPPO eff = SchedulerEffective(step, r, 0, i);
			check(approx(eff.policyLR, r[i].policyLR),
				"degrau: eff.policyLR == resolved[i].policyLR");
			check(eff.epochs == r[i].epochs,
				"degrau: eff.epochs == resolved[i].epochs");
		}
	}

	// ---- 3) SchedulerEffective em modo interpolação (interpolatePPOParams = true) ----
	printf("\n[3] SchedulerEffective (interpolação)\n");
	{
		SchedulerConfig interp = cfg;
		interp.interpolatePPOParams = true;

		// Sem advanceMaxTimesteps nas fases, a interpolação não dispara — deve devolver o degrau
		bool anyHasMax = false;
		for (auto& p : interp.phases)
			if (p.advanceMaxTimesteps) anyHasMax = true;

		if (!anyHasMax) {
			printf("  (nenhuma fase tem advanceMaxTimesteps — interpolação inativa, teste de degrau)\n");
			for (int i = 0; i < nPhases; i++) {
				ResolvedPPO eff = SchedulerEffective(interp, r, 0, i);
				check(approx(eff.policyLR, r[i].policyLR),
					"interp sem timesteps: eff == degrau");
			}
		} else {
			// Se houver fases com timesteps, verifica que os extremos são corretos
			for (int i = 0; i < nPhases; i++) {
				if (!interp.phases[i].advanceMaxTimesteps) continue;
				uint64_t ts = *interp.phases[i].advanceMaxTimesteps;
				ResolvedPPO eff = SchedulerEffective(interp, r, ts, i);
				check(approx(eff.policyLR, r[i].policyLR, 1e-5f),
					"interp no ponto de transicao: policyLR correto");
			}
		}
	}

	// ---- Resumo ----
	printf("\n================================\n");
	if (g_fails == 0)
		printf("TODOS OS %d CHECKS PASSARAM\n", g_checks);
	else
		printf("%d/%d CHECKS FALHARAM\n", g_fails, g_checks);

	return g_fails == 0 ? 0 : 1;
}
