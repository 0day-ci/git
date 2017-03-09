#!/bin/sh

test_description='diff against last branch'

. ./test-lib.sh

test_expect_success 'setup' '
	echo hello >world &&
	git add world &&
	git commit -m initial &&
	git branch other &&
	echo "hello again" >>world &&
	git add world &&
	git commit -m second
'

test_expect_success '"diff -" does not work initially' '
	test_must_fail git diff -
'

test_expect_success '"diff -" diffs against previous branch' '
	git checkout other &&

	cat <<-\EOF >expect &&
	diff --git a/world b/world
	index c66f159..ce01362 100644
	--- a/world
	+++ b/world
	@@ -1,2 +1 @@
	 hello
	-hello again
	EOF

	git diff - >out &&
	test_cmp expect out
'

test_expect_success '"diff -" arguments from stdin' '
	echo "-" | git diff --stdin >out &&
	test_cmp expect out
'

test_expect_success '"diff -.." diffs against previous branch' '
	git diff -.. >out &&
	test_cmp expect out
'

test_expect_success '"diff -.." arguments from stdin' '
	echo "-.." | git diff --stdin >out &&
	test_cmp expect out
'

test_expect_success '"diff ..-" diffs inverted' '
	cat <<-\EOF >expect &&
	diff --git a/world b/world
	index ce01362..c66f159 100644
	--- a/world
	+++ b/world
	@@ -1 +1,2 @@
	 hello
	+hello again
	EOF

	git diff ..- >out &&
	test_cmp expect out
'

test_done
