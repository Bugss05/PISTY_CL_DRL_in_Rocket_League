# Scheduler — Currículo de Treino Automático

O Scheduler muda automaticamente os parâmetros do treino (rewards, state setters, hiperparâmetros do PPO) ao longo do tempo, **sem parar nem recarregar o modelo**. Define fases em `SchedulerConfig.h` e o resto acontece sozinho.

---

## Arquitectura dos ficheiros

| Ficheiro | Responsabilidade |
|---|---|
| `SchedulerConfig.h` | **Único ficheiro que editas.** Define as fases do currículo. |
| `SchedulerCore.h` | Lógica pura: forward-fill de optionals, interpolação linear. Sem dependências de Torch/Learner. |
| `SchedulableState.h` | Substituto do `CombinedState`. Suporta alteração de pesos em runtime. |
| `Scheduler.h / .cpp` | Orquestrador: lê `learner->totalTimesteps`, decide a fase, aplica as alterações. |
| `TrackedStates.h` | State setters que registam que arenas estão em cenários específicos (para métricas). |

---

## Como funciona (fluxo por iteração)

```
StepCallback() chamado a cada step
    │
    └─> g_scheduler.Update(learner)          ← barato, corre 1× por iteração PPO
            │
            ├─ Determina fase pela  totalTimesteps
            ├─ Se fase mudou → ApplyPhaseWeights()   (rewards + state setters, DEGRAU)
            └─ Sempre (ou só na mudança) → ApplyPPO() (LR, entropy, clip, etc.)
                    │
                    ├─ Se interpolatePPOParams=true: interpola linearmente até à fase seguinte
                    └─ Se interpolatePPOParams=false: degrau na fronteira
```

O `Update()` deteta mudança de iteração via `learner->totalIterations` — por isso é seguro chamar a cada step sem custo.

---

## O único ficheiro que precisas de editar: `SchedulerConfig.h`

### Estrutura de uma fase

```cpp
TrainingPhase{
    /*name*/             "NomeDaFase",        // aparece no terminal ao transitar
    /*startTimestep*/    0,                   // quando esta fase começa (totalTimesteps)

    // Parâmetros PPO — std::optional, {} = manter valor da fase anterior
    /*policyLR*/         2e-4f,
    /*criticLR*/         2e-4f,
    /*entropyScale*/     0.035f,
    /*clipRange*/        {},                  // mantém
    /*policyTemperature*/{},                  // mantém
    /*epochs*/           1,                   // SEMPRE em degrau (é inteiro)
    /*gaeGamma*/         0.99f,
    /*gaeLambda*/        {},                  // mantém
    /*rewardClipRange*/  {},                  // mantém

    // Pesos das rewards — por NOME, atualização parcial
    // Rewards não listadas ficam com o peso que tinham
    /*rewardWeights*/    {
        { "Goal",   100.0f },
        { "Air",      0.05f },
    },

    // State setters — por LABEL (o nome que deste no SchedulableState)
    /*stochastic*/       true,                // modo de amostragem
    /*stateWeights*/     {
        { "AirShot",  0.8f },
        { "Kickoff",  0.2f },
    },
},
```

### Flag global `interpolatePPOParams`

```cpp
cfg.interpolatePPOParams = true;  // recomendado
```

| Valor | Comportamento |
|---|---|
| `true` | LR, entropy, clip, temperature, gaeGamma, gaeLambda transitam **linearmente** entre fases. O LR vai de 2e-4 para 1.5e-4 ao longo de 50M steps em vez de saltar. |
| `false` | Todos os parâmetros numéricos mudam em **degrau** na fronteira da fase. |

> `epochs` e os pesos de rewards/state setters mudam **sempre em degrau**, independentemente desta flag.

---

## Forward-fill de optionals

Se uma fase tem `{}` num campo, herda o valor da fase anterior. A fase 0 herda os valores que estavam no `ExampleMain.cpp` quando o `Learner` foi criado.

**Exemplo:**

```
Fase 0: policyLR=2e-4,  entropyScale=0.035
Fase 1: policyLR=1.5e-4, entropyScale={}     <- herda 0.035 da fase 0
Fase 2: policyLR={},     entropyScale=0.018  <- policyLR herda 1.5e-4 da fase 1
```

Resultado resolvido:
```
Fase 0: LR=2e-4,   entropy=0.035
Fase 1: LR=1.5e-4, entropy=0.035  ← forward-fill
Fase 2: LR=1.5e-4, entropy=0.018  ← forward-fill do LR
```

---

## Nomes das rewards

Os nomes no `rewardWeights` têm de coincidir com os definidos em `EnvCreateFunc`:

```cpp
// ExampleMain.cpp — EnvCreateFunc
std::vector<NamedReward> namedRewards = {
    { "VelocityPlayerToBall", new VelocityPlayerToBallReward(), 0.5f },
    { "ConstantPenalty",      new ConstantReward(),            -0.3f },
    { "BallTouchGround",      new BallTouchGroundPenalty(-10.0f), 0.01f },
    { "VelocityBallToGoal",   new VelocityBallToGoalReward(false), 0.8f },
    { "Goal",                 new ZeroSumReward(...),          100.0f },
    { "TouchBallAerial",      new TouchBallAerialReward(),       3.0f },
    { "AirAlignment",         new AirAlignmentReward(),          0.4f },
    { "AerialDistance",       new AerialDistanceReward(),        0.4f },
    { "Air",                  new AirReward(),                  0.05f },
};
```

No `SchedulerConfig.h` usas exactamente esses nomes:

```cpp
/*rewardWeights*/ {
    { "TouchBallAerial", 6.0f },   // correto
    { "TouchBallAerialReward", 6.0f }, // ERRADO — não casa
},
```

> **Atenção:** As rewards não listadas num `rewardWeights` ficam com o peso que tinham — não são zeradas. Se quiseres desativar uma reward, tens de lhe pôr peso `0.0f` explicitamente.

---

## Nomes dos state setters

Os nomes no `stateWeights` têm de coincidir com as labels dadas no `SchedulableState` em `EnvCreateFunc`:

```cpp
// ExampleMain.cpp
SchedulableState* combinedSetter = new SchedulableState({
    { "AirShot",      new AirShotState(),    0.0f },
    { "GoalShot",     new TrackedGoalShotState(), 0.0f },
    { "Random",       new TrackedRandomState(...), 0.0f },
    { "FallingBall",  new TrackedFallingBallApproachState(), 0.0f },
    { "Pass",         new PassState(),        0.0f },
    { "Kickoff",      new KickoffState(),     0.0f },
    { "StaticAerial", new StaticAerialState(), 0.0f },
    { "Cross",        new CrossState(),       0.0f },
    { "WallDrag",     new WallDragState(),    1.0f },
});
```

No `SchedulerConfig.h`:

```cpp
/*stateWeights*/ {
    { "AirShot", 0.8f },  // correto
    { "WallDrag", 0.2f }, // correto
}
```

---

## Modo de amostragem: `stochastic`

```cpp
/*stochastic*/ true,   // cada reset sorteia um cenário com probabilidade proporcional ao peso
/*stochastic*/ false,  // ao longo de N resets, a fracção exacta de cada cenário converge para peso/totalPesos
```

| Modo | Quando usar |
|---|---|
| `true` (stochastic) | Normal — variabilidade natural no treino |
| `false` (deterministic) | Quando queres garantir cobertura exacta de cada cenário (ex.: 30% WallDrag, 70% AirShot sem desvio) |

O modo determinístico usa o algoritmo **largest-remainder**: cada arena acumula um contador e escolhe sempre o setter com maior défice em relação ao alvo.

---

## Continuar de um checkpoint

O Scheduler usa `learner->totalTimesteps` para determinar a fase. Ao continuar de um checkpoint, o `totalTimesteps` já está certo — o Scheduler entra automaticamente na fase correcta sem intervenção manual.

---

## Adicionar uma nova fase

1. Abre `SchedulerConfig.h`
2. Adiciona uma nova entrada ao vector `cfg.phases`, ordenada por `startTimestep` crescente:

```cpp
TrainingPhase{
    /*name*/          "4-FlipReset",
    /*startTimestep*/ 300'000'000,
    /*policyLR*/      8e-5f,
    /*entropyScale*/  0.012f,
    /*epochs*/        2,
    /*gaeGamma*/      0.997f,
    /*rewardWeights*/ {
        { "Goal",           80.0f },
        { "TouchBallAerial", 1.5f },
    },
    /*stateWeights*/  {
        { "AirShot",  0.6f },
        { "GoalShot", 0.4f },
    },
},
```

3. Recompila e corre. Se já tens um checkpoint a partir de 300M steps, a fase activa imediatamente.

---

## Adicionar uma nova reward ao scheduler

1. Em `EnvCreateFunc` (ExampleMain.cpp), adiciona ao `namedRewards`:

```cpp
{ "MeuNome", new MinhaReward(), 1.0f },
```

2. Em `SchedulerConfig.h`, referencia pelo mesmo nome:

```cpp
/*rewardWeights*/ { { "MeuNome", 2.0f } }
```

---

## Adicionar um novo state setter ao scheduler

1. Em `EnvCreateFunc`, adiciona ao `SchedulableState`:

```cpp
{ "MeuCenario", new MeuState(), 0.0f },
```

2. Em `SchedulerConfig.h`:

```cpp
/*stateWeights*/ { { "MeuCenario", 0.5f } }
```

---

## Currículo actual (resumo)

| Fase | Start | Estado principal | Objectivo |
|---|---|---|---|
| `1-TocarBola` | 0 | WallDrag 100% | Aprender a aproximar-se e tocar a bola |
| `2-Remates` | 50M | WallDrag 30%, Kickoff 40%, AirShot 30% | Direcionar a bola à baliza |
| `3-Aereos` | 150M | AirShot 100% | Dominar remates aéreos |

Os parâmetros PPO interpolam linearmente entre fases (ex.: LR de 2e-4 → 1.5e-4 → 1e-4).

---

## O que o Scheduler NÃO faz

- **Não recria modelos nem optimizer.** Os pesos e o estado Adam são preservados entre fases.
- **Não muda `batchSize` nem `miniBatchSize`** — estes são fixos no `ExampleMain.cpp`.
- **Não transita automaticamente** por métricas (ex.: "passa à fase 2 quando touch rate > 80%"). A transição é sempre por `totalTimesteps`. Para transições por métricas, terias de chamar `g_scheduler` manualmente com um `startTimestep` dinâmico.
