#include <linux/ftrace_event.h>
#include <linux/ring_buffer.h>
#include <linux/trace_clock.h>
#include <linux/trace_seq.h>
#include <linux/spinlock.h>
#include <linux/irq_work.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/hardirq.h>
#include <linux/kthread.h>	/* for self test */
#include <linux/kmemcheck.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/hash.h>
#include <linux/list.h>
#include <linux/cpu.h>
#include <linux/fs.h>

#include <asm/local.h>


MODULE_AUTHOR("patrick");
MODULE_LICENSE("Dual BSD/GPL");


/*
 * This is a basic integrity check of the ring buffer.
 * Late in the boot cycle this test will run when configured in.
 * It will kick off a thread per CPU that will go into a loop
 * writing to the per cpu ring buffer various sizes of data.
 * Some of the data will be large items, some small.
 *
 * Another thread is created that goes into a spin, sending out
 * IPIs to the other CPUs to also write into the ring buffer.
 * this is to test the nesting ability of the buffer.
 *
 * Basic stats are recorded and reported. If something in the
 * ring buffer should happen that's not expected, a big warning
 * is displayed and all ring buffers are disabled.
 */
static struct task_struct *rb_threads[NR_CPUS] __initdata;

struct rb_test_data {
	struct ring_buffer	*buffer;
	unsigned long		events;
	unsigned long		bytes_written;
	unsigned long		bytes_alloc;
	unsigned long		bytes_dropped;
	unsigned long		events_nested;
	unsigned long		bytes_written_nested;
	unsigned long		bytes_alloc_nested;
	unsigned long		bytes_dropped_nested;
	int			min_size_nested;
	int			max_size_nested;
	int			max_size;
	int			min_size;
	int			cpu;
	int			cnt;
};

static struct rb_test_data rb_data[NR_CPUS] __initdata;

/* 1 meg per cpu */
#define RB_TEST_BUFFER_SIZE	1048576

static char rb_string[] __initdata =
	"abcdefghijklmnopqrstuvwxyz1234567890!@#$%^&*()?+\\"
	"?+|:';\",.<>/?abcdefghijklmnopqrstuvwxyz1234567890"
	"!@#$%^&*()?+\\?+|:';\",.<>/?abcdefghijklmnopqrstuv";

static bool rb_test_started __initdata;

struct rb_item {
	int size;
	char str[];
};

static __init int rb_write_something(struct rb_test_data *data, bool nested)
{
	struct ring_buffer_event *event;
	struct rb_item *item;
	bool started;
	int event_len;
	int size;
	int len;
	int cnt;

	/* Have nested writes different that what is written */
	cnt = data->cnt + (nested ? 27 : 0);

	/* Multiply cnt by ~e, to make some unique increment */
	size = (data->cnt * 68 / 25) % (sizeof(rb_string) - 1);

	len = size + sizeof(struct rb_item);

	started = rb_test_started;
	/* read rb_test_started before checking buffer enabled */
	smp_rmb();

	event = ring_buffer_lock_reserve(data->buffer, len);
	if (!event) {
		/* Ignore dropped events before test starts. */
		if (started) {
			if (nested)
				data->bytes_dropped += len;
			else
				data->bytes_dropped_nested += len;
		}
		return len;
	}

	event_len = ring_buffer_event_length(event);

	//if (RB_WARN_ON(data->buffer, event_len < len))
	if (event_len < len)
		goto out;

	item = ring_buffer_event_data(event);
	item->size = size;
	memcpy(item->str, rb_string, size);

	if (nested) {
		data->bytes_alloc_nested += event_len;
		data->bytes_written_nested += len;
		data->events_nested++;
		if (!data->min_size_nested || len < data->min_size_nested)
			data->min_size_nested = len;
		if (len > data->max_size_nested)
			data->max_size_nested = len;
	} else {
		data->bytes_alloc += event_len;
		data->bytes_written += len;
		data->events++;
		if (!data->min_size || len < data->min_size)
			data->max_size = len;
		if (len > data->max_size)
			data->max_size = len;
	}

 out:
	ring_buffer_unlock_commit(data->buffer, event);

	return 0;
}

static __init int rb_test(void *arg)
{
	struct rb_test_data *data = arg;

	while (!kthread_should_stop()) {
		rb_write_something(data, false);
		data->cnt++;

		set_current_state(TASK_INTERRUPTIBLE);
		/* Now sleep between a min of 100-300us and a max of 1ms */
		usleep_range(((data->cnt % 3) + 1) * 100, 1000);
	}

	return 0;
}

static __init void rb_ipi(void *ignore)
{
	struct rb_test_data *data;
	int cpu = smp_processor_id();

	data = &rb_data[cpu];
	rb_write_something(data, true);
}

static __init int rb_hammer_test(void *arg)
{
	while (!kthread_should_stop()) {

		/* Send an IPI to all cpus to write data! */
		smp_call_function(rb_ipi, NULL, 1);
		/* No sleep, but for non preempt, let others run */
		schedule();
	}

	return 0;
}

static __init int test_ringbuffer(void)
{
	struct task_struct *rb_hammer;
	struct ring_buffer *buffer;
	int cpu;
	int ret = 0;

	pr_info("Running ring buffer tests...\n");

	buffer = ring_buffer_alloc(RB_TEST_BUFFER_SIZE, RB_FL_OVERWRITE);
	if (WARN_ON(!buffer))
		return 0;

	/* Disable buffer so that threads can't write to it yet */
	ring_buffer_record_off(buffer);

	for_each_online_cpu(cpu) {
		rb_data[cpu].buffer = buffer;
		rb_data[cpu].cpu = cpu;
		rb_data[cpu].cnt = cpu;
		rb_threads[cpu] = kthread_create(rb_test, &rb_data[cpu],
						 "rbtester/%d", cpu);
		if (WARN_ON(!rb_threads[cpu])) {
			pr_cont("FAILED\n");
			ret = -1;
			goto out_free;
		}

		kthread_bind(rb_threads[cpu], cpu);
 		wake_up_process(rb_threads[cpu]);
	}

	/* Now create the rb hammer! */
	rb_hammer = kthread_run(rb_hammer_test, NULL, "rbhammer");
	if (WARN_ON(!rb_hammer)) {
		pr_cont("FAILED\n");
		ret = -1;
		goto out_free;
	}

	ring_buffer_record_on(buffer);
	/*
	 * Show buffer is enabled before setting rb_test_started.
	 * Yes there's a small race window where events could be
	 * dropped and the thread wont catch it. But when a ring
	 * buffer gets enabled, there will always be some kind of
	 * delay before other CPUs see it. Thus, we don't care about
	 * those dropped events. We care about events dropped after
	 * the threads see that the buffer is active.
	 */
	smp_wmb();
	rb_test_started = true;

	set_current_state(TASK_INTERRUPTIBLE);
	/* Just run for 10 seconds */;
	schedule_timeout(10 * HZ);

	kthread_stop(rb_hammer);

 out_free:
	for_each_online_cpu(cpu) {
		if (!rb_threads[cpu])
			break;
		kthread_stop(rb_threads[cpu]);
	}
	if (ret) {
		ring_buffer_free(buffer);
		return ret;
	}

	/* Report! */
	pr_info("finished\n");
	for_each_online_cpu(cpu) {
		struct ring_buffer_event *event;
		struct rb_test_data *data = &rb_data[cpu];
		struct rb_item *item;
		unsigned long total_events;
		unsigned long total_dropped;
		unsigned long total_written;
		unsigned long total_alloc;
		unsigned long total_read = 0;
		unsigned long total_size = 0;
		unsigned long total_len = 0;
		unsigned long total_lost = 0;
		unsigned long lost;
		int big_event_size;
		int small_event_size;

		ret = -1;

		total_events = data->events + data->events_nested;
		total_written = data->bytes_written + data->bytes_written_nested;
		total_alloc = data->bytes_alloc + data->bytes_alloc_nested;
		total_dropped = data->bytes_dropped + data->bytes_dropped_nested;

		big_event_size = data->max_size + data->max_size_nested;
		small_event_size = data->min_size + data->min_size_nested;

		pr_info("CPU %d:\n", cpu);
		pr_info("              events:    %ld\n", total_events);
		pr_info("       dropped bytes:    %ld\n", total_dropped);
		pr_info("       alloced bytes:    %ld\n", total_alloc);
		pr_info("       written bytes:    %ld\n", total_written);
		pr_info("       biggest event:    %d\n", big_event_size);
		pr_info("      smallest event:    %d\n", small_event_size);

		//if (RB_WARN_ON(buffer, total_dropped))
		if (total_dropped)
			break;

		ret = 0;

		while ((event = ring_buffer_consume(buffer, cpu, NULL, &lost))) {
			total_lost += lost;
			item = ring_buffer_event_data(event);
			total_len += ring_buffer_event_length(event);
			total_size += item->size + sizeof(struct rb_item);
			if (memcmp(&item->str[0], rb_string, item->size) != 0) {
				pr_info("FAILED!\n");
				pr_info("buffer had: %.*s\n", item->size, item->str);
				pr_info("expected:   %.*s\n", item->size, rb_string);
				//RB_WARN_ON(buffer, 1);
				ret = -1;
				break;
			}
			total_read++;
		}
		if (ret)
			break;

		ret = -1;

		pr_info("         read events:   %ld\n", total_read);
		pr_info("         lost events:   %ld\n", total_lost);
		pr_info("        total events:   %ld\n", total_lost + total_read);
		pr_info("  recorded len bytes:   %ld\n", total_len);
		pr_info(" recorded size bytes:   %ld\n", total_size);
		if (total_lost)
			pr_info(" With dropped events, record len and size may not match\n"
				" alloced and written from above\n");
		if (!total_lost) {
			//if (RB_WARN_ON(buffer, total_len != total_alloc ||
			//	       total_size != total_written))
			if ((total_len != total_alloc) || (total_size != total_written))
				break;
		}
		//if (RB_WARN_ON(buffer, total_lost + total_read != total_events))
		if ((total_lost + total_read) != total_events)
			break;

		ret = 0;
	}
	if (!ret)
		pr_info("Ring buffer PASSED!\n");

	ring_buffer_free(buffer);
	return 0;
}


module_init(test_ringbuffer);
//module_exit(my_seq_exit);
