#!/bin/sh
#
# Copyright (c) 2009 Ilari Liusvaara
#

test_description='Test run command'

. ./test-lib.sh

cat >hello-script <<-EOF
	#!$SHELL_PATH
	cat hello-script
EOF
>empty

test_expect_success 'start_command reports ENOENT' '
	test-run-command start-command-ENOENT ./does-not-exist
'

test_expect_success 'run_command can run a command' '
	cat hello-script >hello.sh &&
	chmod +x hello.sh &&
	test-run-command run-command ./hello.sh >actual 2>err &&

	test_cmp hello-script actual &&
	test_cmp empty err
'

test_expect_success 'run_command should not try to execute a directory' '
	test_when_finished "rm -rf bin1 bin2 bin3" &&
	mkdir -p bin1/greet bin2 bin3 &&
	write_script bin2/greet <<-\EOF &&
	cat bin2/greet
	EOF
	chmod -x bin2/greet &&
	write_script bin3/greet <<-\EOF &&
	cat bin3/greet
	EOF

	# Test that run-command does not try to execute the "greet" directory in
	# "bin1", or the non-executable file "greet" in "bin2", but rather
	# correcty executes the "greet" script located in bin3.
	(
		PATH=$PWD/bin1:$PWD/bin2:$PWD/bin3:$PATH &&
		export PATH &&
		test-run-command run-command greet >actual 2>err
	) &&
	test_cmp bin3/greet actual &&
	test_cmp empty err
'

test_expect_success POSIXPERM 'run_command reports EACCES' '
	cat hello-script >hello.sh &&
	chmod -x hello.sh &&
	test_must_fail test-run-command run-command ./hello.sh 2>err &&

	grep "fatal: cannot exec.*hello.sh" err
'

test_expect_success POSIXPERM,SANITY 'unreadable directory in PATH' '
	mkdir local-command &&
	test_when_finished "chmod u+rwx local-command && rm -fr local-command" &&
	git config alias.nitfol "!echo frotz" &&
	chmod a-rx local-command &&
	(
		PATH=./local-command:$PATH &&
		git nitfol >actual
	) &&
	echo frotz >expect &&
	test_cmp expect actual
'

cat >expect <<-EOF
preloaded output of a child
Hello
World
preloaded output of a child
Hello
World
preloaded output of a child
Hello
World
preloaded output of a child
Hello
World
EOF

test_expect_success 'run_command runs in parallel with more jobs available than tasks' '
	test-run-command run-command-parallel 5 sh -c "printf \"%s\n%s\n\" Hello World" 2>actual &&
	test_cmp expect actual
'

test_expect_success 'run_command runs in parallel with as many jobs as tasks' '
	test-run-command run-command-parallel 4 sh -c "printf \"%s\n%s\n\" Hello World" 2>actual &&
	test_cmp expect actual
'

test_expect_success 'run_command runs in parallel with more tasks than jobs available' '
	test-run-command run-command-parallel 3 sh -c "printf \"%s\n%s\n\" Hello World" 2>actual &&
	test_cmp expect actual
'

cat >expect <<-EOF
preloaded output of a child
asking for a quick stop
preloaded output of a child
asking for a quick stop
preloaded output of a child
asking for a quick stop
EOF

test_expect_success 'run_command is asked to abort gracefully' '
	test-run-command run-command-abort 3 false 2>actual &&
	test_cmp expect actual
'

cat >expect <<-EOF
no further jobs available
EOF

test_expect_success 'run_command outputs ' '
	test-run-command run-command-no-jobs 3 sh -c "printf \"%s\n%s\n\" Hello World" 2>actual &&
	test_cmp expect actual
'

test_done
