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

- [KernelSU](https://t.me/KernelSU_group/3234/500071)
- [KernelSU-Next](https://t.me/ksunext_ci/15235)
- [BakaSU](https://t.me/BakaSU_Grp/4/342071?single)
- [FinaSU](https://t.me/FinaSU/72)

## 📝 Changelog

**SUperRyzeNS kernel +11 changes**

### Scheduler
- BORE scheduler updated to 7.0.1r2, the base tuning this build runs on.
- schedutil no longer reuses `cached_raw_freq` across policy limit changes, so the
  selected frequency stays correct after a limit is raised or lowered.

### Memory and storage
- Critical tasks skip direct reclaim throttling, so latency-sensitive work no
  longer waits behind background reclaim.
- Slab shrinking is skipped for critical tasks too, keeping caches warm under
  memory pressure without stalling the system.
- binder uses dedicated slab caches for the hot transaction objects, cutting
  allocation contention on the busiest path.
- f2fs NAT cache enlarged so both the 6GB and 8GB variants keep their node cache
  resident.
- sdcardfs allocates the lookup name only after a match, removing an allocation
  from the negative-lookup path.

### Display
- drm/msm recycles atomic allocations instead of allocating per commit, making
  atomic commits cheaper.
- dsi-staging stops the FIFO error interrupts from storming the interrupt path
  and the kernel log.

### Power and input
- Charger and fuel-gauge work moved to a power-efficient workqueue, so polling
  no longer keeps a busy CPU awake.
- The gt9896 touch controller IRQ is quiesced while the controller is in reset,
  avoiding spurious interrupts during recovery.

---

## ⚠️ Important Note

- Always backup before flashing.
- If bootloop change ROM.
- **DWYOR!** (Do With Your Own Risk)
