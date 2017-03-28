#!/bin/sh
#
# Copyright (c) 2017 Jacob Keller
#

test_description='Test diff tab expansion

'
. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

test_expect_success '--diff-expand-tabs setup' '
	git reset --hard &&
	{
		echo "0. no tabs"
	} >x &&
	git update-index --add x &&
	git commit -a --allow-empty -m preimage &&
	{
		echo "0.	some	tabs"
	} >x &&

	cat >expect.default <<-\EOF &&
	diff --git a/x b/x
	index e4d15da..410478b 100644
	--- a/x
	+++ b/x
	@@ -1 +1 @@
	-0. no tabs
	+0.      some    tabs
	EOF

	cat >expect.size-12 <<-\EOF &&
	diff --git a/x b/x
	index e4d15da..410478b 100644
	--- a/x
	+++ b/x
	@@ -1 +1 @@
	-0. no tabs
	+0.          some        tabs
	EOF

	cat >expect.size-0 <<-\EOF
	diff --git a/x b/x
	index e4d15da..410478b 100644
	--- a/x
	+++ b/x
	@@ -1 +1 @@
	-0. no tabs
	+0.	some	tabs
	EOF
'

test_expect_success 'test --diff-expand-tabs option' '
	git diff --diff-expand-tabs >actual &&
	test_cmp expect.default actual &&

	git diff --diff-expand-tabs=12 >actual &&
	test_cmp expect.size-12 actual &&

	git diff --diff-expand-tabs=0 >actual &&
	test_cmp expect.size-0 actual
'

test_done
