#!/bin/sh

test_description="Tests performance of read-tree"

. ./perf-lib.sh

test_perf_default_repo
test_checkout_worktree

## usage: dir depth width files
make_paths () {
	for f in $(seq $4)
	do
		echo $1/file$f
	done;
	if test $2 -gt 0;
	then
		for w in $(seq $3)
		do
			make_paths $1/dir$w $(($2 - 1)) $3 $4
		done
	fi
	return 0
}

fill_index () {
	make_paths $1 $2 $3 $4 |
	sed "s/^/100644 $EMPTY_BLOB	/" |
	git update-index --index-info
	return 0
}

br_base=xxx_base_xxx
br_work=xxx_work_xxx

new_dir=xxx_dir_xxx

## (5, 10, 9) will create 999,999 files.
## (4, 10, 9) will create  99,999 files.
depth=5
width=10
files=9

export br_base
export br_work
export new_dir
export depth
export width
export files

## The number of files in the xxx_base_xxx branch.
nr_base=$(git ls-files | wc -l)
export nr_base

## Inflate the index with thousands of empty files and commit it.
## Turn on sparse-checkout so that we don't have to populate them
## later when we start switching branches.
test_expect_success 'inflate the index' '
	git reset --hard &&
	git branch $br_base &&
	git branch $br_work &&
	git checkout $br_work &&
	fill_index $new_dir $depth $width $files &&
	git commit -m $br_work &&
	echo $new_dir/file1 >.git/info/sparse-checkout &&
	git config --local core.sparsecheckout 1 &&
	git reset --hard
'

## The number of files in the xxx_work_xxx branch.
nr_work=$(git ls-files | wc -l)
export nr_work

test_perf "read-tree ($nr_work)" '
	git read-tree -m $br_base $br_work -n
'

test_perf "switch branches ($nr_base $nr_work)" '
	git checkout $br_base &&
	git checkout $br_work
'


test_done
