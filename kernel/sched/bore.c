/*
 *  Burst-Oriented Response Enhancer (BORE) CPU Scheduler
 *  Copyright (C) 2021-2025 Masahito Suzuki <firelzrd@gmail.com>
 *
 *  Backport of BORE 7.0.1 for the 4.14 based EEVDF scheduler.
 */
#include <linux/cpuset.h>
#include <linux/math64.h>
#include <linux/sched/task.h>
#include <linux/sched/bore.h>
#include "sched.h"

#ifdef CONFIG_SCHED_BORE
DEFINE_STATIC_KEY_TRUE(sched_bore_key);
unsigned int __read_mostly sched_bore			= 1;
unsigned int __read_mostly sched_burst_inherit_type	= 2;
unsigned int __read_mostly sched_burst_smoothness	= 1;
/*
 * Bursts below 2^26 ns (67 ms) are not penalised at all. The upstream
 * 2^24 ns (16.8 ms) threshold is shorter than a handful of base slices
 * on this tree (3.33 ms at HZ=300), so UI and render threads that run
 * for tens of milliseconds while scrolling were being treated as bursty
 * and lost weight against short sleepers. Mobile workloads keep the
 * discrimination for real CPU hogs, which run far longer than 67 ms
 * without sleeping.
 */
unsigned int __read_mostly sched_burst_penalty_offset	= 26;
unsigned int __read_mostly sched_burst_penalty_scale	= 1536;
unsigned int __read_mostly sched_burst_cache_lifetime	= 75000000;

/*
 * Cap the sleep credit at 4 ms instead of 16 ms. On wakeup the credit is
 * subtracted from the virtual deadline, so the old cap let a thread that
 * slept through screen-off jump up to ~5 base slices ahead of everything
 * that woke with it. One slice worth of credit is enough to win the
 * wakeup race without letting long sleepers reorder the runqueue.
 */
unsigned int __read_mostly sched_credit_cap_us		= 4000;

#define MAX_BURST_PENALTY ((40U << 8) - 1)
#define MAXVAL_PRIO 39
#define BURST_CACHE_SAMPLE_LIMIT 63
#define BURST_CACHE_SCAN_LIMIT (BURST_CACHE_SAMPLE_LIMIT * 2)

static u32 bore_reciprocal_lut[BURST_CACHE_SAMPLE_LIMIT + 1];

DEFINE_STATIC_KEY_TRUE(sched_burst_inherit_key);
DEFINE_STATIC_KEY_TRUE(sched_burst_ancestor_key);
DEFINE_STATIC_KEY_FALSE(sched_credit_key);

static inline u32 log2p1_u64_u32fp(u64 v, u8 fp)
{
	int exponent;
	u32 mantissa;

	if (unlikely(!v))
		return 0;

	exponent = fls64(v);
	mantissa = (u32)((v << (64 - exponent)) << 1 >> (64 - fp));

	return (u32)exponent << fp | mantissa;
}

void bore_note_sleep(struct task_struct *p, u64 now)
{
	p->bore.credit_sleep = now;
}

u64 bore_credit_ns(struct task_struct *p)
{
	u64 cap = (u64)sched_credit_cap_us * 1000ULL;
	u64 slept = 0, credit = 0;

	if (p->bore.credit_sleep) {
		slept = rq_clock(task_rq(p)) - p->bore.credit_sleep;
		if ((s64)slept > 0) {
			p->bore.credit_sleep = 0;
			credit = slept < cap ? slept : cap;
		}
	}

	return credit;
}

static inline u32 calc_burst_penalty(u64 burst_time)
{
	u32 greed = log2p1_u64_u32fp(burst_time, 8);
	u32 tolerance = sched_burst_penalty_offset << 8;
	s32 diff = (s32)(greed - tolerance);
	u32 penalty = diff & ~(diff >> 31);
	u32 scaled_penalty = penalty * sched_burst_penalty_scale >> 10;
	s32 overflow = scaled_penalty - MAX_BURST_PENALTY;

	return scaled_penalty - (overflow & ~(overflow >> 31));
}

static inline u64 rescale_slice(u64 delta, u8 old_prio, u8 new_prio)
{
	u64 unscaled, rescaled;

	unscaled = mul_u64_u32_shr(delta, sched_prio_to_weight[old_prio], 10);
	rescaled = mul_u64_u32_shr(unscaled, sched_prio_to_wmult[new_prio], 22);

	return rescaled;
}

static inline u32 binary_smooth(u32 new, u32 old)
{
	u32 is_growing = (new > old);
	u32 increment = (new - old) * is_growing;
	u32 shift = sched_burst_smoothness;
	u32 smoothed = old + ((increment + (1U << shift) - 1) >> shift);

	return (new & ~(-is_growing)) | (smoothed & (-is_growing));
}

static inline struct cfs_rq *bore_task_cfs_rq(struct task_struct *p)
{
#ifdef CONFIG_FAIR_GROUP_SCHED
	return p->se.cfs_rq;
#else
	return &task_rq(p)->cfs;
#endif
}

static void reweight_task_by_prio(struct task_struct *p, int prio)
{
	struct sched_entity *se = &p->se;
	unsigned long weight = scale_load(sched_prio_to_weight[prio]);

	if (idle_policy(p->policy))
		return;

	if (se->on_rq) {
		p->bore.stop_update = true;
		reweight_entity(bore_task_cfs_rq(p), se, weight);
		p->bore.stop_update = false;
	} else
		se->load.weight = weight;
	se->load.inv_weight = sched_prio_to_wmult[prio];
}

u8 effective_prio_bore(struct task_struct *p)
{
	int prio = p->static_prio - MAX_RT_PRIO;
	s32 diff;

	if (static_branch_likely(&sched_bore_key))
		prio += bore_score(p);
	prio &= ~(prio >> 31);
	diff = prio - MAXVAL_PRIO;
	prio -= (diff & ~(diff >> 31));

	return (u8)prio;
}

static void update_penalty(struct task_struct *p)
{
	struct bore_ctx *ctx = &p->bore;
	u8  prev_prio = effective_prio_bore(p);
	s32 diff = (s32)ctx->curr_penalty - (s32)ctx->prev_penalty;
	u16 max_val = ctx->curr_penalty - (diff & (diff >> 31));
	u32 is_kthread = !!(p->flags & PF_KTHREAD);
	u8  new_prio;

	ctx->penalty = max_val & -(s32)(!is_kthread);

	new_prio = effective_prio_bore(p);
	if (new_prio != prev_prio)
		reweight_task_by_prio(p, new_prio);
}

void update_curr_bore(struct task_struct *p, u64 delta_exec)
{
	struct bore_ctx *ctx = &p->bore;
	u32 curr_penalty;

	if (ctx->stop_update)
		return;

	ctx->burst_time += delta_exec;
	curr_penalty = ctx->curr_penalty = calc_burst_penalty(ctx->burst_time);

	if (curr_penalty <= ctx->prev_penalty)
		return;
	update_penalty(p);
}

void restart_burst_bore(struct task_struct *p)
{
	struct bore_ctx *ctx = &p->bore;
	u32 new_penalty = binary_smooth(ctx->curr_penalty, ctx->prev_penalty);

	ctx->prev_penalty = new_penalty;
	ctx->curr_penalty = 0;
	ctx->burst_time = 0;
	update_penalty(p);
}

void restart_burst_rescale_deadline_bore(struct task_struct *p)
{
	struct sched_entity *se = &p->se;
	s64 vremain = se->deadline - se->vruntime;
	s64 vscaled;
	u8 old_prio = effective_prio_bore(p);
	u8 new_prio;

	restart_burst_bore(p);
	new_prio = effective_prio_bore(p);

	if (old_prio > new_prio) {
		vscaled = rescale_slice(vremain < 0 ? -vremain : vremain,
					old_prio, new_prio);
		if (unlikely(vremain < 0))
			vscaled = -vscaled;
		se->deadline = se->vruntime + vscaled;
	}
}

static inline bool task_is_bore_eligible(struct task_struct *p)
{
	return p && p->sched_class == &fair_sched_class && !p->exit_state;
}

#ifndef for_each_child_task
#define for_each_child_task(p, t) \
	list_for_each_entry_rcu(t, &(p)->children, sibling)
#endif

static inline u32 count_children_upto2(struct task_struct *p)
{
	struct task_struct *child;
	u32 count = 0;

	list_for_each_entry_rcu(child, &p->children, sibling) {
		if (++count >= 2)
			break;
	}

	return count;
}

static inline bool burst_cache_expired(struct bore_bc *bc, u64 now)
{
	struct bore_bc bc_val = { .value = READ_ONCE(bc->value) };
	u64 timestamp = (u64)bc_val.timestamp << BORE_BC_TIMESTAMP_SHIFT;

	return now - timestamp > (u64)sched_burst_cache_lifetime;
}

static void update_burst_cache(struct bore_bc *bc, struct task_struct *p,
			       u32 count, u32 total, u64 now)
{
	u32 average = (count == 1) ? total :
		(u32)(((u64)total * bore_reciprocal_lut[count]) >> 32);
	struct bore_bc new_bc = {
		.penalty = max(average, (u32)p->bore.penalty),
		.timestamp = now >> BORE_BC_TIMESTAMP_SHIFT
	};

	WRITE_ONCE(bc->value, new_bc.value);
}

static u32 inherit_from_parent(struct task_struct *parent,
			       u64 clone_flags, u64 now)
{
	struct bore_bc bc_val;
	struct bore_bc *bc;

	if (clone_flags & CLONE_PARENT)
		parent = rcu_dereference(parent->real_parent);

	bc = &parent->bore.subtree;

	if (burst_cache_expired(bc, now)) {
		struct task_struct *child;
		u32 count = 0, total = 0, scan_count = 0;

		for_each_child_task(parent, child) {
			if (count >= BURST_CACHE_SAMPLE_LIMIT)
				break;
			if (scan_count++ >= BURST_CACHE_SCAN_LIMIT)
				break;

			if (!task_is_bore_eligible(child))
				continue;
			count++;
			total += child->bore.penalty;
		}

		update_burst_cache(bc, parent, count, total, now);
	}

	bc_val.value = READ_ONCE(bc->value);

	return (u32)bc_val.penalty;
}

static u32 inherit_from_ancestor_hub(struct task_struct *parent,
				     u64 clone_flags, u64 now)
{
	struct bore_bc bc_val;
	struct task_struct *ancestor = parent;
	struct task_struct *next;
	u32 sole_child_count = 0;
	struct bore_bc *bc;

	if (clone_flags & CLONE_PARENT) {
		ancestor = rcu_dereference(ancestor->real_parent);
		sole_child_count = 1;
	}

	for (next = rcu_dereference(ancestor->real_parent);
	     next != ancestor && count_children_upto2(ancestor) <= sole_child_count;
	     ancestor = next, sole_child_count = 1,
	     next = rcu_dereference(ancestor->real_parent)) {
	}

	bc = &ancestor->bore.subtree;

	if (burst_cache_expired(bc, now)) {
		struct task_struct *direct_child;
		u32 count = 0, total = 0, scan_count = 0;

		for_each_child_task(ancestor, direct_child) {
			struct task_struct *descendant = direct_child;

			if (count >= BURST_CACHE_SAMPLE_LIMIT)
				break;
			if (scan_count++ >= BURST_CACHE_SCAN_LIMIT)
				break;

			while (count_children_upto2(descendant) == 1) {
				struct task_struct *next_descendant =
					list_first_or_null_rcu(
						&descendant->children,
						struct task_struct, sibling);
				if (!next_descendant)
					break;
				descendant = next_descendant;
			}

			if (!task_is_bore_eligible(descendant))
				continue;
			count++;
			total += descendant->bore.penalty;
		}

		update_burst_cache(bc, ancestor, count, total, now);
	}

	bc_val.value = READ_ONCE(bc->value);

	return (u32)bc_val.penalty;
}

static u32 inherit_from_thread_group(struct task_struct *p, u64 now)
{
	struct bore_bc bc_val;
	struct task_struct *leader = p->group_leader;
	struct bore_bc *bc = &leader->bore.group;

	if (burst_cache_expired(bc, now)) {
		struct task_struct *sibling;
		u32 count = 0, total = 0, scan_count = 0;

		for_each_thread(leader, sibling) {
			if (count >= BURST_CACHE_SAMPLE_LIMIT)
				break;
			if (scan_count++ >= BURST_CACHE_SCAN_LIMIT)
				break;

			if (!task_is_bore_eligible(sibling))
				continue;
			count++;
			total += sibling->bore.penalty;
		}

		update_burst_cache(bc, leader, count, total, now);
	}

	bc_val.value = READ_ONCE(bc->value);

	return (u32)bc_val.penalty;
}

void task_fork_bore(struct task_struct *p, struct task_struct *parent,
		    u64 clone_flags, u64 now)
{
	struct bore_ctx *ctx = &p->bore;
	u32 inherited_penalty;

	if (!static_branch_likely(&sched_bore_key) || !task_is_bore_eligible(p))
		return;

	rcu_read_lock();
	if (clone_flags & CLONE_THREAD)
		inherited_penalty = inherit_from_thread_group(parent, now);
	else if (static_branch_likely(&sched_burst_inherit_key))
		inherited_penalty = static_branch_likely(&sched_burst_ancestor_key) ?
			inherit_from_ancestor_hub(parent, clone_flags, now) :
			inherit_from_parent(parent, clone_flags, now);
	else
		inherited_penalty = 0;

	if (ctx->prev_penalty < inherited_penalty)
		ctx->prev_penalty = inherited_penalty;
	ctx->curr_penalty  = 0;
	ctx->burst_time    = 0;
	ctx->credit_sleep  = 0;
	ctx->stop_update   = false;
	ctx->futex_waiting = false;
	update_penalty(p);
	rcu_read_unlock();
}

void reset_task_bore(struct task_struct *p)
{
	memset(&p->bore, 0, sizeof(struct bore_ctx));
}

static void update_inherit_type(void)
{
	switch (sched_burst_inherit_type) {
	case 1:
		static_branch_enable(&sched_burst_inherit_key);
		static_branch_disable(&sched_burst_ancestor_key);
		break;
	case 2:
		static_branch_enable(&sched_burst_inherit_key);
		static_branch_enable(&sched_burst_ancestor_key);
		break;
	default:
		static_branch_disable(&sched_burst_inherit_key);
		break;
	}
}

void __init sched_init_bore(void)
{
	u32 i;

	printk(KERN_INFO "%s %s by %s\n",
		SCHED_BORE_PROGNAME, SCHED_BORE_VERSION, SCHED_BORE_AUTHOR);

	bore_reciprocal_lut[0] = 0;
	for (i = 1; i <= BURST_CACHE_SAMPLE_LIMIT; i++)
		bore_reciprocal_lut[i] = (u32)div64_u64(0xffffffffULL + i, i);

	reset_task_bore(&init_task);
	update_inherit_type();

	if (sched_credit_cap_us)
		static_branch_enable(&sched_credit_key);
}

static void readjust_all_task_weights(void)
{
	struct task_struct *task;
	struct rq *rq;
	struct rq_flags rf;

	write_lock_irq(&tasklist_lock);
	for_each_process(task) {
		if (!task_is_bore_eligible(task))
			continue;
		rq = task_rq_lock(task, &rf);
		update_rq_clock(rq);
		reweight_task_by_prio(task, effective_prio_bore(task));
		task_rq_unlock(rq, task, &rf);
	}
	write_unlock_irq(&tasklist_lock);
}

int sched_bore_update_handler(struct ctl_table *table, int write,
			      void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		return ret;

	if (sched_bore)
		static_branch_enable(&sched_bore_key);
	else
		static_branch_disable(&sched_bore_key);

	readjust_all_task_weights();

	return 0;
}

int sched_burst_inherit_type_update_handler(struct ctl_table *table, int write,
					    void __user *buffer, size_t *lenp,
					    loff_t *ppos)
{
	int ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		return ret;

	update_inherit_type();

	return 0;
}

int sched_credit_cap_us_update_handler(struct ctl_table *table, int write,
				       void __user *buffer, size_t *lenp,
				       loff_t *ppos)
{
	int ret = proc_douintvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		return ret;

	if (sched_credit_cap_us)
		static_branch_enable(&sched_credit_key);
	else
		static_branch_disable(&sched_credit_key);

	return 0;
}
#endif /* CONFIG_SCHED_BORE */
