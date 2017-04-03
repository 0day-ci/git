#!/bin/sh

test_description="Tests performance of reading the index"

. ./perf-lib.sh

test_perf_default_repo

count=1000
test_perf "read_cache/discard_cache checksum=1 $count times" "
	git config --local core.checksumindex 1 &&
	test-read-cache $count
"

test_perf "read_cache/discard_cache checksum=0 $count times" "
	git config --local core.checksumindex 0 &&
	test-read-cache $count
"

test_done
