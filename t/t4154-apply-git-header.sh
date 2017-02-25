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

test_expect_success 'apply deleted file mode / new file mode / wrong mode' '
	test_must_fail git apply << EOF
diff --git a/. b/.
deleted file mode 
new file mode 
EOF
'

test_expect_success 'apply deleted file mode / new file mode / wrong type' '
	mkdir x &&
	chmod 755 x &&
	test_must_fail git apply << EOF
diff --git a/x b/x
deleted file mode 160755
new file mode 
EOF
'

test_expect_success 'apply deleted file mode / new file mode / already exists' '
	touch 1 &&
	chmod 644 1 &&
	test_must_fail git apply << EOF
diff --git a/1 b/1
deleted file mode 100644
new file mode 
EOF
'

test_expect_success 'apply new file mode / copy from / nonexistant file' '
	test_must_fail git apply << EOF
diff --git a/. b/.
new file mode 
copy from  
EOF
'

test_done
