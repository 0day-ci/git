#!/bin/sh

test_description='apply with git/--git headers'

. ./test-lib.sh

test_expect_success 'apply old mode / rename new' '
	test_must_fail git apply << EOF
diff --git a/1 b/1
old mode 0
rename new 0
EOF
'

test_done
