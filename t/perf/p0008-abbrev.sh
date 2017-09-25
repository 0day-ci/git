#!/bin/bash

test_description='Test object disambiguation through abbreviations'
. ./perf-lib.sh

test_perf_large_repo

test-list-objects 100000 | sort -R > objs.txt

test_perf 'find_unique_abbrev() for existing objects' '
	test-abbrev < objs.txt
'

test-list-objects 100000 --missing | sort -R > objs.txt

test_perf 'find_unique_abbrev() for missing objects' '
	test-abbrev < objs.txt
'

rm objs.txt

test_done
