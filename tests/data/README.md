# 回归样本清单

reference-manifest.json 登记 F01–F17 待采集项以及现有三个谱图/两个参考表的哈希。全部 expected 和 tolerances 为 null。不是已验证的黄金样本，不改变源样例或表。

numerical-reference-manifest.json 登记 S4 的 N01–N08 数值参考采集任务、具体场景及逐量纲比较目标。expected/environment 仍为 null；proposed_tolerances 不是已签定的 Fortran 兼容界限，legacy_tolerances_frozen=false。独立 C++ 单测的解析期望值另在测试源码中，不混入此参考清单。
