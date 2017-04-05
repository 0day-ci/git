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

br_work1=xxx_work1_xxx

new_dir=xxx_dir_xxx

## (5, 10, 9) will create 999,999 files.
## (4, 10, 9) will create  99,999 files.
depth=5
width=10
files=9

export br_work1

export new_dir

export depth
export width
export files

## Inflate the index with thousands of empty files and commit it.
test_expect_success 'inflate the index' '
	git reset --hard &&
	git branch $br_work1 &&
	git checkout $br_work1 &&
	fill_index $new_dir $depth $width $files &&
	git commit -m $br_work1 &&
	git reset --hard
'

## The number of files in the xxx_work1_xxx branch.
nr_work1=$(git ls-files | wc -l)
export nr_work1

test_perf "read-tree status work1 ($nr_work1)" '
	git read-tree HEAD &&
	git status
'

test_done
