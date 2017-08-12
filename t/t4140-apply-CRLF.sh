#!/bin/sh

test_description='CRLF diff and apply'

. ./test-lib.sh

test_expect_success setup '
	mkdir upstream &&
	(
		cd upstream &&
		git init &&
		git config core.autocrlf false &&
		>.gitignore &&
		git add . &&
		git commit -m gitignore &&
		printf "F1\r\n" >FileCRLF &&
		git add . &&
		git commit -m initial &&
		git diff HEAD^1 HEAD -- >../patch1
	) &&
	git config core.autocrlf true
'

test_expect_success 'apply patches Replace lines' '
	(
		cd upstream &&
		printf "F11\r\nF12\r\n" >FileCRLF &&
		git diff  >../patch2Replace
	) &&
	git apply patch1 &&
	git apply patch2Replace
'

test_expect_success 'apply patches Add lines' '
	(
		cd upstream &&
		git checkout FileCRLF &&
		printf "F2\r\n" >>FileCRLF &&
		git diff  >../patch2Add
	) &&
	rm -f FileCRLF &&
	git apply patch1 &&
	git apply patch2Add
'

test_done
