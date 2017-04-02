#!/bin/sh

test_description='Test directory iteration.'

. ./test-lib.sh

cat >expect-sorted-output <<-\EOF &&
[d] (a) ./dir/a
[d] (a/b) ./dir/a/b
[d] (a/b/c) ./dir/a/b/c
[d] (d) ./dir/d
[d] (d/e) ./dir/d/e
[d] (d/e/d) ./dir/d/e/d
[f] (a/b/c/d) ./dir/a/b/c/d
[f] (a/e) ./dir/a/e
[f] (b) ./dir/b
[f] (c) ./dir/c
[f] (d/e/d/a) ./dir/d/e/d/a
EOF

test_expect_success 'dir-iterator should iterate through all files' '
	mkdir -p dir &&
	mkdir -p dir/a/b/c/ &&
	>dir/b &&
	>dir/c &&
	mkdir -p dir/d/e/d/ &&
	>dir/a/b/c/d &&
	>dir/a/e &&
	>dir/d/e/d/a &&

	test-dir-iterator ./dir | sort >./actual-pre-order-sorted-output &&
	rm -rf dir &&

	test_cmp expect-sorted-output actual-pre-order-sorted-output
'

cat >expect-pre-order-output <<-\EOF &&
[d] (a) ./dir/a
[d] (a/b) ./dir/a/b
[d] (a/b/c) ./dir/a/b/c
[f] (a/b/c/d) ./dir/a/b/c/d
EOF

test_expect_success 'dir-iterator should list files in the correct order' '
	mkdir -p dir/a/b/c/ &&
	>dir/a/b/c/d &&

	test-dir-iterator ./dir >actual-pre-order-output &&
	rm -rf dir &&

	test_cmp expect-pre-order-output actual-pre-order-output
'

test_done
