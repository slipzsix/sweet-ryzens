# 🦊**SUperRyzeNS**™
<img width="2730" height="1536" alt="SUperRyzeNS" src="{{BANNER}}" />

### Release {{TAG}}
A [**STRIX**](https://github.com/fiqri19102002/android_kernel_xiaomi_sweet/tree/disp-param) based costum kernel tuned for smooth performance and daily use, stable memory and ram management for better gaming experience.

## ✨ Features
⚙️ **CPU & Frequency**
- Available governors: schedutil (default), performance, powersave, Moonbeam, Reflex.
- PELT with a 16 ms half-life (WALT disabled).
- Input boost 100 ms → 1.017 GHz (little) / 1.094 GHz (big).
- Wake boost 1 second when the screen wakes.
- Fork/zygote boost 50 ms → 1.804 GHz / 2.304 GHz + DDR/LLCC boost to 2597.
-  Kprofiles: 4 modes (0 = off, 1 = battery, 2 = balanced, 3 = performance) via /sys/
       kernel/kprofiles/kp_mode; automatically switches to battery mode when the screen turns
       off.
     - Battery mode forces powersave and disables input/devfreq boosts.
     - 300 Hz tick, full preemption, NO_HZ_IDLE.
 
⚙️ **CPU — tunable**
- Governors (/sys/devices/system/cpu/cpufreq/policy*/scaling_governor)
- schedutil (default), performance, powersave, moonbeam, reflex.
- schedutil rate limits (.../policy*/schedutil/)
     - up_rate_limit_us, down_rate_limit_us.
- Kprofiles — 4 kernel profiles
     - /sys/kernel/kprofiles/kp_mode → 0 off, 1 battery, 2 balanced, 3 performance.
     - Auto-switch to battery when screen turns off
- Automatic boosts (built-in, no knob)
     - Input boost, wake boost, zygote/fork boost, DDR/LLCC boost, memory-pressure boost.
      - Battery profile automatically disables input/devfreq boosts.

🧠 **Scheduler — tunable**
- BORE can be switched at runtime:
      - /proc/sys/kernel/sched_bore → 0/1.
- Built-in scheduling features
     - EEVDF fair scheduler, CASS capacity-aware balancing, uclamp assist.
     - Tuned cpusets for foreground/background/top-app/camera-daemon.

🎮 **GPU — tunable**
- Governor (/sys/class/kgsl/kgsl-3d0/devfreq/governor)
     - msm-adreno-tz (default), powersave, performance.
- Built-in: ULPS, disabled KGSL throttling, Adreno 6xx-only driver.

🧩 **Memory & ZRAM — tunable**
- ZRAM size — /sys/block/zram0/disksize
     - Default 3 GB on first init.
     - After reset, userspace writes are honoured, so it can be resized.
     - Typical flow: swapoff /dev/zram0 → echo 1 > /sys/block/zram0/reset → echo 4G > /sys/
       block/zram0/disksize → mkswap → swapon.
- ZRAM compression — /sys/block/zram0/comp_algorithm
     - Available: lz4, lz4kdr (default), lzo, deflate.
- Built-in memory features
     - MGLRU v13.
     - simple_lmk low-memory killer.
     - ZRAM per-CPU compression streams + entropy calculation.
     - THP, MEMCG, and PSI disabled.

💾 **Storage & I/O — tunable**
- I/O scheduler — /sys/block/<dev>/queue/scheduler
     - UFS (blk-mq): none, mq-deadline, kyber.
     - Legacy block devices: zen, cfq, deadline, noop.
     - BFQ is not shipped.
- Built-in filesystem support
     - F2FS, EXT4, EROFS, exFAT, SDCardFS, FUSE, overlayfs.

🔌 **Charging & Power — tunable**
- Force fast charge
     - /sys/kernel/fast_charge/force_fast_charge → 0/1 (default 1).
- Charging enable/disable
     - /sys/class/power_supply/battery/battery_charging_enabled → 0/1.
- Built-in
     - LN8000 + bq2597x charge pump.
     - USB-PD and QC/PD authentication.
     - Qualcomm lpm-levels cpuidle.

🌡️ **Thermal — tunable**
- Writable thermal trip points
     - /sys/class/thermal/thermal_zone*/trip_point_*_temp (where exposed by the zone).
- Built-in
     - step_wise thermal governor.
     - mi_thermald-compatible dummy nodes.
     - DCVS/LMH disabled.

🌐 **Network — tunable**
- TCP congestion control
     - /proc/sys/net/ipv4/tcp_congestion_control
     - Available: bbrplus (default), bbr, cubic, westwood.
- Queue discipline
     - Default fq_codel; other qdiscs can be set with tc.
- Built-in
     - WireGuard.
     - WPA3 SAE.
     - iptables TTL target (no TTL/HL reduction for tethering).
     - WiFi packet injection.

♻️ **KernelSU**
- SUSFS v2.4.0 support
- NoMount support
- KernelSU with multi-manager compatibility
- use NSU if you want APatch or Magisk to root
---
 ## 🆕 {{TAG}} Changelog
- Scheduler
     - BORE scheduler updated to 7.0.1r2, the base tuning this build runs on.
     - schedutil no longer reuses `cached_raw_freq` across policy limit changes, so the selected frequency stays correct after a limit is raised or lowered.
- Memory and storage
     - sdcardfs allocates the lookup name only after a match, removing an allocation
  from the negative-lookup path.
- Display
     - drm/msm recycles atomic allocations instead of allocating per commit, making
  atomic commits cheaper.
- Power
     - Charger and fuel-gauge work moved to a power-efficient workqueue, so polling
  no longer keeps a busy CPU awake.
- Note
     - {{TAG}} updates the [staging](https://t.me/superryzens/324) build previously shared in our Telegram group.
---
## 📦 Variants
| Variant | ROM  |   Root
| --- | --- | --- |
| **MIUI** | MIUI or HyperOS | Included |
| **LEGACY** | if not booted with MIUI variant | Included |
| **AOSP** | for AOSP Roms | Included |
| **AOSP NSU** | Non-SU/Non-KSU variant | Not Included |

## 🧩 Managers Support
| Manager | Version | Code | Spoof |
| --- |  --- |  --- | --- |
[**KernelSU**](https://t.me/KernelSU_group/3234/500579) | v3.3.1 | 32735 | Not available |
[**KernelSU-Next**](https://t.me/ksunext_ci/15240) | v3.4.2 | 33646 | Available 33646 Only |
[**BakaSU**](https://t.me/BakaSU_Grp/4/346348?single) | v4.2.0 | 35282 | Not available |
[**FinaSU**](https://t.me/FinaSU/84) | v1.7.0 | 33224 |  Not available |

## ⚠️ Important Note
Flash the SUperRyzeNS zip for your chosen variant. Make sure to back up boot and dtbo first before flashing in case anything goes wrong. If bootloop change ROM. **DWYOR!**

**Author:** [𝙎𝙡𝙞𝙥𝙯 𝙎𝙄𝙓](https://t.me/slipzsix)  
**Group:** [𝙎𝙍 𝙋𝙂 𝙎𝙪𝙥𝙥𝙤𝙧𝙩](https://t.me/superryzensplayground)

[<img width="177" height="28" alt="Buy Me A Coffee" src="https://img.shields.io/badge/%E2%98%95-Buy%20Me%20a%20Coffee-orange?style=for-the-badge" />](https://sociabuzz.com/slipzsix/tribe)


