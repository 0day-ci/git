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

	test-dir-iterator ./dir 1 | sort >./actual-pre-order-sorted-output &&
	rm -rf dir &&

	test_cmp expect-sorted-output actual-pre-order-sorted-output
'

test_expect_success 'dir-iterator should iterate through all files on post-order mode' '
	mkdir -p dir &&
	mkdir -p dir/a/b/c/ &&
	>dir/b &&
	>dir/c &&
	mkdir -p dir/d/e/d/ &&
	>dir/a/b/c/d &&
	>dir/a/e &&
	>dir/d/e/d/a &&

	test-dir-iterator ./dir 2 | sort >actual-post-order-sorted-output &&
	rm -rf dir &&

	test_cmp expect-sorted-output actual-post-order-sorted-output
'

cat >expect-pre-order-output <<-\EOF &&
[d] (a) ./dir/a
[d] (a/b) ./dir/a/b
[d] (a/b/c) ./dir/a/b/c
[f] (a/b/c/d) ./dir/a/b/c/d
EOF

test_expect_success 'dir-iterator should list files properly on pre-order mode' '
	mkdir -p dir/a/b/c/ &&
	>dir/a/b/c/d &&

	test-dir-iterator ./dir 1 >actual-pre-order-output &&
	rm -rf dir &&

	test_cmp expect-pre-order-output actual-pre-order-output
'

cat >expect-post-order-output <<-\EOF &&
[f] (a/b/c/d) ./dir/a/b/c/d
[d] (a/b/c) ./dir/a/b/c
[d] (a/b) ./dir/a/b
[d] (a) ./dir/a
EOF

test_expect_success 'dir-iterator should list files properly on post-order mode' '
	mkdir -p dir/a/b/c/ &&
	>dir/a/b/c/d &&

	test-dir-iterator ./dir 2 >actual-post-order-output &&
	rm -rf dir &&

	test_cmp expect-post-order-output actual-post-order-output
'

cat >expect-pre-order-post-order-root-dir-output <<-\EOF &&
[d] (.) ./dir
[d] (a) ./dir/a
[d] (a/b) ./dir/a/b
[d] (a/b/c) ./dir/a/b/c
[f] (a/b/c/d) ./dir/a/b/c/d
[d] (a/b/c) ./dir/a/b/c
[d] (a/b) ./dir/a/b
[d] (a) ./dir/a
[d] (.) ./dir
EOF

test_expect_success 'dir-iterator should list files properly on pre-order + post-order + root-dir mode' '
	mkdir -p dir/a/b/c/ &&
	>dir/a/b/c/d &&

	test-dir-iterator ./dir 7 >actual-pre-order-post-order-root-dir-output &&
	rm -rf dir &&

	test_cmp expect-pre-order-post-order-root-dir-output actual-pre-order-post-order-root-dir-output
'

test_done
