# 逻辑综合优化 TODO

## 阶段一：验证 fraig 效果（今天完成）

### 1.1 单命令快速验证
对 v1z 完全无效的6个顽固用例逐一跑：

```bash
# 在 abc 交互模式下
read_blif tc_07/input.blif; strash; fraig; ps
read_blif tc_09/input.blif; strash; fraig; ps
read_blif tc_16/input.blif; strash; fraig; ps
read_blif tc_17/input.blif; strash; fraig; ps
read_blif tc_23/input.blif; strash; fraig; ps
read_blif tc_26/input.blif; strash; fraig; ps
```

**记录每个用例的 AND 节点数，与 noopt（strash only）对比。**

---

### 1.2 全量30用例跑新序列
准备以下两个脚本对比：

**脚本 A（当前 v1z）：**
```
read_blif <case>/input.blif; strash; b; rewrite; refactor; b; rewrite -lz; b; refactor -z; resub; b; rewrite -lz; refactor -z; b; ps
```

**脚本 B（v1z + fraig）：**
```
read_blif <case>/input.blif; strash; fraig; b; rewrite; refactor; b; rewrite -lz; b; refactor -z; resub; b; rewrite -lz; refactor -z; b; fraig; b; ps
```

批量执行并收集结果，重点看：
- [ ] tc_07 / tc_09 / tc_16 / tc_17 / tc_23 / tc_26 节点数有没有下降
- [ ] 其余24个用例节点数有没有退步（fraig 不应该让节点数变多）

---

### 1.3 等价性验证
对有变化的用例跑 CEC：

```bash
read_blif tc_XX/input.blif; strash; write_blif /tmp/orig.blif
read_blif tc_XX/input.blif; strash; fraig; ...; write_blif /tmp/opt.blif
cec /tmp/orig.blif /tmp/opt.blif
```

确认全部 PASS 才进入下一阶段。

---

## 阶段二：根据阶段一结果决策

### 情况 A：fraig 对顽固用例有效
- [ ] 把 fraig 固定进参赛脚本序列
- [ ] 测试不同位置插入 fraig 的效果（前、中、后、多次）
- [ ] 比较 `fraig` vs `fraig_sweep`（后者更快但可能效果差一点）

### 情况 B：fraig 也无效
- [ ] 用 ABC 的 `print_stats` / `print_sharing` 分析顽固用例结构
- [ ] 看那6个用例的 I/O 特征，判断是什么类型的电路
- [ ] 回来讨论下一步方向

---

## 阶段三：优化序列调优（阶段二完成后）

在确定 fraig 有效的基础上，测试更激进的序列：

```bash
# 候选序列 C：多次迭代
strash; fraig; resyn2; fraig; resyn2; fraig; b

# 候选序列 D：含 resubstitution
strash; fraig; resyn2rs; fraig; resyn2rs; fraig; b
```

对全部30用例跑，记录：
- [ ] 总 AND 节点数
- [ ] 总运行时间
- [ ] 有无 crash 或 CEC 失败

---

## 关键指标记录表

| 流程        | 总 AND 节点数 | 相比 noopt | 相比 v1z |
| ----------- | ------------- | ---------- | -------- |
| noopt       | 60331         | —          | —        |
| v1z（当前） | 48984         | -18.8%     | —        |
| v1z + fraig | ?             | ?          | ?        |
| 候选序列 C  | ?             | ?          | ?        |
| 候选序列 D  | ?             | ?          | ?        |

---

## 注意事项

- 每次改序列后必须跑全部30用例的 CEC，不能只跑有效果的用例
- 运行时间也是评分项（20%），序列不能无限堆叠
- `fraig` 在大电路（tc_22：11367节点）上可能较慢，注意计时