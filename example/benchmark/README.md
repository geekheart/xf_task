# Benchmark 示例

**目标**
验证调度器在大量任务下的吞吐上限，统计每秒执行次数与单任务平均执行频率。

**原理**
通过创建大量 `ttask` / `ntask` / `ctask`，在固定周期内执行轻量逻辑，并按秒统计执行次数。配置通过 `benchmark_config.h` 的宏控制，避免运行时参数干扰。

**如何编译**
`xmake build benchmark`

**如何运行**
`xmake run benchmark`

**运行结果**
终端会输出 `bench_config` 配置行，以及每秒一次的 `exec_rate`、`per_task` 等统计值。通过调整 `example/benchmark/benchmark_config.h` 中的宏可对比不同模式与参数的吞吐差异。
