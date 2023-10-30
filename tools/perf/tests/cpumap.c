// SPDX-License-Identifier: GPL-2.0
#include "tests.h"
#include <stdio.h>
#include "cpumap.h"
#include "event.h"
#include "util/synthetic-events.h"
#include <string.h>
#include <linux/bitops.h>
#include <perf/cpumap.h>
#include "debug.h"

struct machine;

static int process_event_mask(struct perf_tool *tool __maybe_unused,
			 union perf_event *event,
			 struct perf_sample *sample __maybe_unused,
			 struct machine *machine __maybe_unused)
{
	struct perf_record_cpu_map *map_event = &event->cpu_map;
	struct perf_record_cpu_map_data *data;
	struct perf_cpu_map *map;
	unsigned int long_size;

	data = &map_event->data;

	TEST_ASSERT_VAL("wrong type", data->type == PERF_CPU_MAP__MASK);

	long_size = data->mask32_data.long_size;

	TEST_ASSERT_VAL("wrong long_size", long_size == 4 || long_size == 8);

	TEST_ASSERT_VAL("wrong nr",   data->mask32_data.nr == 1);

	TEST_ASSERT_VAL("wrong cpu", perf_record_cpu_map_data__test_bit(0, data));
	TEST_ASSERT_VAL("wrong cpu", !perf_record_cpu_map_data__test_bit(1, data));
	for (int i = 2; i <= 20; i++)
		TEST_ASSERT_VAL("wrong cpu", perf_record_cpu_map_data__test_bit(i, data));

	map = cpu_map__new_data(data);
	TEST_ASSERT_VAL("wrong nr",  perf_cpu_map__nr(map) == 20);

	TEST_ASSERT_VAL("wrong cpu", perf_cpu_map__cpu(map, 0).cpu == 0);
	for (int i = 2; i <= 20; i++)
		TEST_ASSERT_VAL("wrong cpu", perf_cpu_map__cpu(map, i - 1).cpu == i);

	perf_cpu_map__put(map);
	return 0;
}

static int process_event_cpus(struct perf_tool *tool __maybe_unused,
			 union perf_event *event,
			 struct perf_sample *sample __maybe_unused,
			 struct machine *machine __maybe_unused)
{
	struct perf_record_cpu_map *map_event = &event->cpu_map;
	struct perf_record_cpu_map_data *data;
	struct perf_cpu_map *map;

	data = &map_event->data;

	TEST_ASSERT_VAL("wrong type", data->type == PERF_CPU_MAP__CPUS);

	TEST_ASSERT_VAL("wrong nr",   data->cpus_data.nr == 2);
	TEST_ASSERT_VAL("wrong cpu",  data->cpus_data.cpu[0] == 1);
	TEST_ASSERT_VAL("wrong cpu",  data->cpus_data.cpu[1] == 256);

	map = cpu_map__new_data(data);
	TEST_ASSERT_VAL("wrong nr",  perf_cpu_map__nr(map) == 2);
	TEST_ASSERT_VAL("wrong cpu", perf_cpu_map__cpu(map, 0).cpu == 1);
	TEST_ASSERT_VAL("wrong cpu", perf_cpu_map__cpu(map, 1).cpu == 256);
	TEST_ASSERT_VAL("wrong refcnt", refcount_read(&map->refcnt) == 1);
	perf_cpu_map__put(map);
	return 0;
}

static int process_event_range_cpus(struct perf_tool *tool __maybe_unused,
				union perf_event *event,
				struct perf_sample *sample __maybe_unused,
				struct machine *machine __maybe_unused)
{
	struct perf_record_cpu_map *map_event = &event->cpu_map;
	struct perf_record_cpu_map_data *data;
	struct perf_cpu_map *map;

	data = &map_event->data;

	TEST_ASSERT_VAL("wrong type", data->type == PERF_CPU_MAP__RANGE_CPUS);

	TEST_ASSERT_VAL("wrong any_cpu",   data->range_cpu_data.any_cpu == 0);
	TEST_ASSERT_VAL("wrong start_cpu", data->range_cpu_data.start_cpu == 1);
	TEST_ASSERT_VAL("wrong end_cpu",   data->range_cpu_data.end_cpu == 256);

	map = cpu_map__new_data(data);
	TEST_ASSERT_VAL("wrong nr",  perf_cpu_map__nr(map) == 256);
	TEST_ASSERT_VAL("wrong cpu", perf_cpu_map__cpu(map, 0).cpu == 1);
	TEST_ASSERT_VAL("wrong cpu", perf_cpu_map__max(map).cpu == 256);
	TEST_ASSERT_VAL("wrong refcnt", refcount_read(&map->refcnt) == 1);
	perf_cpu_map__put(map);
	return 0;
}


static int test__cpu_map_synthesize(struct test_suite *test __maybe_unused, int subtest __maybe_unused)
{
	struct perf_cpu_map *cpus;

	/* This one is better stored in a mask. */
	cpus = perf_cpu_map__new("0,2-20");

	TEST_ASSERT_VAL("failed to synthesize map",
		!perf_event__synthesize_cpu_map(NULL, cpus, process_event_mask, NULL));

	perf_cpu_map__put(cpus);

	/* This one is better stored in cpu values. */
	cpus = perf_cpu_map__new("1,256");

	TEST_ASSERT_VAL("failed to synthesize map",
		!perf_event__synthesize_cpu_map(NULL, cpus, process_event_cpus, NULL));

	perf_cpu_map__put(cpus);

	/* This one is better stored as a range. */
	cpus = perf_cpu_map__new("1-256");

	TEST_ASSERT_VAL("failed to synthesize map",
		!perf_event__synthesize_cpu_map(NULL, cpus, process_event_range_cpus, NULL));

	perf_cpu_map__put(cpus);
	return 0;
}

static int cpu_map_print(const char *str)
{
	struct perf_cpu_map *map = perf_cpu_map__new(str);
	char buf[100];

	if (!map)
		return -1;

	cpu_map__snprint(map, buf, sizeof(buf));
	perf_cpu_map__put(map);

	return !strcmp(buf, str);
}

static int test__cpu_map_print(struct test_suite *test __maybe_unused, int subtest __maybe_unused)
{
	TEST_ASSERT_VAL("failed to convert map", cpu_map_print("1"));
	TEST_ASSERT_VAL("failed to convert map", cpu_map_print("1,5"));
	TEST_ASSERT_VAL("failed to convert map", cpu_map_print("1,3,5,7,9,11,13,15,17,19,21-40"));
	TEST_ASSERT_VAL("failed to convert map", cpu_map_print("2-5"));
	TEST_ASSERT_VAL("failed to convert map", cpu_map_print("1,3-6,8-10,24,35-37"));
	TEST_ASSERT_VAL("failed to convert map", cpu_map_print("1,3-6,8-10,24,35-37"));
	TEST_ASSERT_VAL("failed to convert map", cpu_map_print("1-10,12-20,22-30,32-40"));
	return 0;
}

static int test__cpu_map_merge(struct test_suite *test __maybe_unused, int subtest __maybe_unused)
{
	struct perf_cpu_map *a = perf_cpu_map__new("4,2,1");
	struct perf_cpu_map *b = perf_cpu_map__new("4,5,7");
	struct perf_cpu_map *c = perf_cpu_map__merge(a, b);
	char buf[100];

	TEST_ASSERT_VAL("failed to merge map: bad nr", perf_cpu_map__nr(c) == 5);
	cpu_map__snprint(c, buf, sizeof(buf));
	TEST_ASSERT_VAL("failed to merge map: bad result", !strcmp(buf, "1-2,4-5,7"));
	perf_cpu_map__put(b);
	perf_cpu_map__put(c);
	return 0;
}

DEFINE_SUITE("Synthesize cpu map", cpu_map_synthesize);
DEFINE_SUITE("Print cpu map", cpu_map_print);
DEFINE_SUITE("Merge cpu map", cpu_map_merge);
