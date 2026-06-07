// Teste standalone da lógica do scheduler (sem torch, sem GPU, sem Learner).
// Exercita EXATAMENTE as funções de SchedulerCore.h usadas em produção.
//
// Compilar/correr via CMake:   cmake --build build --config Release --target scheduler_test
//                              build\Release\scheduler_test.exe

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

static bool approx(float a, float b, float tol) {
	return std::fabs(a - b) <= tol;
}

int main() {
	SchedulerConfig cfg = GetSchedulerConfig();
	printf("Currículo com %d fases, interpolatePPOParams=%d\n\n",
		(int)cfg.phases.size(), (int)cfg.interpolatePPOParams);

	// Pré-requisito do teste: assume o currículo de exemplo (3 fases @ 0 / 50M / 150M).
	check(cfg.phases.size() == 3, "currículo tem 3 fases");

	// ---- 1) Deteção de fase por timestep ----
	printf("[1] PhaseFor (fronteiras de fase)\n");
	check(SchedulerPhaseFor(cfg, 0) == 0,            "ts=0 -> fase 0");
	check(SchedulerPhaseFor(cfg, 49'999'999) == 0,   "ts=49.999.999 -> fase 0");
	check(SchedulerPhaseFor(cfg, 50'000'000) == 1,   "ts=50M (exato) -> fase 1");
	check(SchedulerPhaseFor(cfg, 149'999'999) == 1,  "ts=149.999.999 -> fase 1");
	check(SchedulerPhaseFor(cfg, 150'000'000) == 2,  "ts=150M (exato) -> fase 2");
	check(SchedulerPhaseFor(cfg, 1'000'000'000) == 2,"ts=1B -> fase 2 (última)");

	// ---- 2) Forward-fill dos optionals ----
	// Baseline com sentinelas distintas para verificar herança de campos vazios ({}).
	ResolvedPPO baseline{};
	baseline.policyLR = 9; baseline.criticLR = 9; baseline.entropyScale = 9;
	baseline.clipRange = 0.111f; baseline.policyTemperature = 0.222f; baseline.epochs = 99;
	baseline.gaeGamma = 9; baseline.gaeLambda = 0.333f; baseline.rewardClipRange = 0.444f;

	std::vector<ResolvedPPO> r = SchedulerResolvePhases(cfg, baseline);

	printf("\n[2] ResolvePhases (forward-fill)\n");
	check(approx(r[0].policyLR, 2e-4f, 1e-9f),  "fase0.policyLR = 2e-4 (definido)");
	check(approx(r[1].policyLR, 1.5e-4f, 1e-9f),"fase1.policyLR = 1.5e-4");
	check(approx(r[2].policyLR, 1e-4f, 1e-9f),  "fase2.policyLR = 1e-4");
	check(approx(r[0].gaeGamma, 0.99f, 1e-6f),  "fase0.gaeGamma = 0.99");
	check(approx(r[1].gaeGamma, 0.993f, 1e-6f), "fase1.gaeGamma = 0.993");
	check(approx(r[2].gaeGamma, 0.995f, 1e-6f), "fase2.gaeGamma = 0.995");
	// Campos vazios em TODAS as fases -> herdam a baseline.
	check(approx(r[0].clipRange, 0.111f, 1e-9f),        "fase0.clipRange vazio -> baseline (0.111)");
	check(approx(r[2].clipRange, 0.111f, 1e-9f),        "fase2.clipRange ainda herda baseline");
	check(approx(r[1].policyTemperature, 0.222f, 1e-9f),"fase1.policyTemperature -> baseline (0.222)");
	check(approx(r[2].gaeLambda, 0.333f, 1e-9f),        "fase2.gaeLambda -> baseline (0.333)");
	// epochs definido só na fase 0 (=1) -> forward-fill para as seguintes (NÃO a baseline 99).
	check(r[0].epochs == 1, "fase0.epochs = 1 (definido)");
	check(r[1].epochs == 1, "fase1.epochs vazio -> herda fase0 (=1), não a baseline");
	check(r[2].epochs == 1, "fase2.epochs vazio -> herda (=1)");

	// ---- 3) Interpolação (interpolatePPOParams = true) ----
	printf("\n[3] Interpolação contínua\n");
	auto interpAt = [&](uint64_t ts) {
		return SchedulerInterpolate(cfg, r, ts, SchedulerPhaseFor(cfg, ts));
	};
	check(approx(interpAt(0).policyLR, 2e-4f, 1e-9f),
		"ts=0 -> policyLR = 2e-4 (início da fase 0)");
	check(approx(interpAt(25'000'000).policyLR, 1.75e-4f, 1e-9f),
		"ts=25M (meio fase0->1) -> policyLR = 1.75e-4 (média)");
	check(approx(interpAt(50'000'000).policyLR, 1.5e-4f, 1e-9f),
		"ts=50M -> policyLR = 1.5e-4 (início da fase 1)");
	check(approx(interpAt(100'000'000).policyLR, 1.25e-4f, 1e-9f),
		"ts=100M (meio fase1->2) -> policyLR = 1.25e-4 (média)");
	check(approx(interpAt(25'000'000).gaeGamma, 0.9915f, 1e-6f),
		"ts=25M -> gaeGamma = 0.9915 (média 0.99/0.993)");
	check(approx(interpAt(150'000'000).policyLR, 1e-4f, 1e-9f),
		"ts=150M -> policyLR = 1e-4 (última fase, sem interpolação)");
	check(approx(interpAt(500'000'000).policyLR, 1e-4f, 1e-9f),
		"ts=500M -> policyLR = 1e-4 (mantém-se na última fase)");
	check(interpAt(25'000'000).epochs == 1,
		"epochs nunca é interpolado (fica em degrau)");

	// ---- 4) Modo degrau (interpolatePPOParams = false) ----
	printf("\n[4] Modo degrau (sem interpolação)\n");
	SchedulerConfig step = cfg;
	step.interpolatePPOParams = false;
	auto stepAt = [&](uint64_t ts) {
		return SchedulerInterpolate(step, r, ts, SchedulerPhaseFor(step, ts));
	};
	check(approx(stepAt(25'000'000).policyLR, 2e-4f, 1e-9f),
		"ts=25M -> policyLR = 2e-4 (valor da fase 0, sem interpolar)");
	check(approx(stepAt(100'000'000).policyLR, 1.5e-4f, 1e-9f),
		"ts=100M -> policyLR = 1.5e-4 (valor da fase 1)");

	// ---- Resumo ----
	printf("\n================================\n");
	if (g_fails == 0)
		printf("TODOS OS %d CHECKS PASSARAM ✔\n", g_checks);
	else
		printf("%d/%d CHECKS FALHARAM ✘\n", g_fails, g_checks);

	return g_fails == 0 ? 0 : 1;
}
