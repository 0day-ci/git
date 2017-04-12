#!/bin/sh

test_description='Test ls-files recurse-submodules feature

This test verifies the recurse-submodules feature correctly lists files across
submodules.
'

. ./test-lib.sh

test_expect_success 'setup directory structure and submodule' '
	echo "foobar" >a &&
	mkdir b &&
	echo "bar" >b/b &&
	git add a b &&
	git commit -m "add a and b" &&
	git init submodule &&
	echo "foobar" >submodule/a &&
	git -C submodule add a &&
	git -C submodule commit -m "add a" &&
	git submodule add ./submodule &&
	git commit -m "added submodule"
'

test_expect_success 'ls-files correctly lists files in a submodule' '
	cat >expect <<-\EOF &&
	.gitmodules
	a
	b/b
	submodule/a
	EOF

	git ls-files --recurse-submodules >actual &&
	test_cmp expect actual
'

test_expect_success 'ls-files and basic pathspecs' '
	cat >expect <<-\EOF &&
	submodule/a
	EOF

	git ls-files --recurse-submodules -- submodule >actual &&
	test_cmp expect actual
'

test_expect_success 'ls-files and nested submodules' '
	git init submodule/sub &&
	echo "foobar" >submodule/sub/a &&
	git -C submodule/sub add a &&
	git -C submodule/sub commit -m "add a" &&
	git -C submodule submodule add ./sub &&
	git -C submodule add sub &&
	git -C submodule commit -m "added sub" &&
	git add submodule &&
	git commit -m "updated submodule" &&

	cat >expect <<-\EOF &&
	.gitmodules
	a
	b/b
	submodule/.gitmodules
	submodule/a
	submodule/sub/a
	EOF

	git ls-files --recurse-submodules >actual &&
	test_cmp expect actual
'

test_expect_success 'ls-files using relative path' '
	test_when_finished "rm -rf parent sub" &&
	git init sub &&
	echo "foobar" >sub/file &&
	git -C sub add file &&
	git -C sub commit -m "add file" &&

	git init parent &&
	echo "foobar" >parent/file &&
	git -C parent add file &&
	mkdir parent/src &&
	echo "foobar" >parent/src/file2 &&
	git -C parent add src/file2 &&
	git -C parent submodule add ../sub &&
	git -C parent commit -m "add files and submodule" &&

	# From top works
	cat >expect <<-\EOF &&
	.gitmodules
	file
	src/file2
	sub/file
	EOF
	git -C parent ls-files --recurse-submodules >actual &&
	test_cmp expect actual &&

	# Relative path to top
	cat >expect <<-\EOF &&
	../.gitmodules
	../file
	file2
	../sub/file
	EOF
	git -C parent/src ls-files --recurse-submodules .. >actual &&
	test_cmp expect actual &&

	# Relative path to submodule
	cat >expect <<-\EOF &&
	../sub/file
	EOF
	git -C parent/src ls-files --recurse-submodules ../sub >actual &&
	test_cmp expect actual
'

test_expect_success 'ls-files from a subdir' '
	test_when_finished "rm -rf parent sub" &&
	git init sub &&
	echo "foobar" >sub/file &&
	git -C sub add file &&
	git -C sub commit -m "add file" &&

	git init parent &&
	mkdir parent/src &&
	echo "foobar" >parent/src/file &&
	git -C parent add src/file &&
	git -C parent submodule add ../sub src/sub &&
	git -C parent submodule add ../sub sub &&
	git -C parent commit -m "add files and submodules" &&

	# Verify grep from root works
	cat >expect <<-\EOF &&
	.gitmodules
	src/file
	src/sub/file
	sub/file
	EOF
	git -C parent ls-files --recurse-submodules >actual &&
	test_cmp expect actual &&

	# Verify grep from a subdir works
	cat >expect <<-\EOF &&
	file
	sub/file
	EOF
	git -C parent/src ls-files --recurse-submodules >actual &&
	test_cmp expect actual
'

test_incompatible_with_recurse_submodules ()
{
	test_expect_success "--recurse-submodules and $1 are incompatible" "
		test_must_fail git ls-files --recurse-submodules $1 2>actual &&
		test_i18ngrep -- '--recurse-submodules unsupported mode' actual
	"
}

test_incompatible_with_recurse_submodules --deleted
test_incompatible_with_recurse_submodules --others
test_incompatible_with_recurse_submodules --unmerged
test_incompatible_with_recurse_submodules --killed
test_incompatible_with_recurse_submodules --modified

test_done
