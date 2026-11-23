# 🦊SUperRyzeNS™ Kernel

![SUperRyzeNS]({{BANNER}})

## 📦 {{TAG}}

| Item | Detail |
| --- | --- |
| Release tag | `{{TAG}}` |
| Kernel | `{{KERNEL}}` |
| Build time | {{BUILD_TIME}} |
| Device | Redmi Note 10 Pro (sweet) |

## 🛠️ Kernel managers

- [KernelSU](https://t.me/KernelSU_group/3234/500579)
- [KernelSU-Next](https://t.me/ksunext_ci/15240)
- [BakaSU](https://t.me/BakaSU_Grp/4/346348?single)
- [FinaSU](https://t.me/FinaSU/84)

## 📝 Changelog

**SUperRyzeNS kernel +5 changes**

### Scheduler
- BORE scheduler updated to 7.0.1r2, the base tuning this build runs on.
- schedutil no longer reuses `cached_raw_freq` across policy limit changes, so the
  selected frequency stays correct after a limit is raised or lowered.

### Memory and storage
- sdcardfs allocates the lookup name only after a match, removing an allocation
  from the negative-lookup path.

### Display
- drm/msm recycles atomic allocations instead of allocating per commit, making
  atomic commits cheaper.

### Power and input
- Charger and fuel-gauge work moved to a power-efficient workqueue, so polling
  no longer keeps a busy CPU awake.

---

## ⚠️ Important Note

- Always backup before flashing.
- If bootloop change ROM.
- **DWYOR!** (Do With Your Own Risk)

---

**Author:** [𝙎𝙡𝙞𝙥𝙯 𝙎𝙄𝙓](https://t.me/slipzsix)  
**Group:** [SR PG Support](https://t.me/superryzensplayground)

[![Buy Me A Coffee](https://superryzens.sourceforge.net/assets/bmc-200.png)](https://sociabuzz.com/slipzsix/tribe)
