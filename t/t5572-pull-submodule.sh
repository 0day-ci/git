#!/bin/sh

test_description='pull can handle submodules'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-submodule-update.sh

reset_branch_to_HEAD () {
	git branch -D "$1" &&
	git checkout -b "$1" HEAD &&
	git branch --set-upstream-to="origin/$1" "$1"
}

git_pull () {
	reset_branch_to_HEAD "$1" &&
	git pull
}

# pulls without conflicts
test_submodule_switch "git_pull"

git_pull_ff () {
	reset_branch_to_HEAD "$1" &&
	git pull --ff
}

test_submodule_switch "git_pull_ff"

git_pull_ff_only () {
	reset_branch_to_HEAD "$1" &&
	git pull --ff-only
}

test_submodule_switch "git_pull_ff_only"

git_pull_noff () {
	reset_branch_to_HEAD "$1" &&
	git pull --no-ff
}

KNOWN_FAILURE_NOFF_MERGE_DOESNT_CREATE_EMPTY_SUBMODULE_DIR=1
KNOWN_FAILURE_NOFF_MERGE_ATTEMPTS_TO_MERGE_REMOVED_SUBMODULE_FILES=1
test_submodule_switch "git_pull_noff"

test_expect_success 'pull --recurse-submodule setup' '
	git init child &&
	(
		cd child &&
		echo "bar" >file &&
		git add file &&
		git commit -m "initial commit"
	) &&
	git init parent &&
	(
		cd parent &&
		echo "foo" >file &&
		git add file &&
		git commit -m "Initial commit" &&
		git submodule add ../child sub &&
		git commit -m "add submodule"
	) &&
	git clone --recurse-submodule parent super &&
	git -C super/sub checkout master
'

test_expect_success 'pull recursive fails without --rebase' '
	test_must_fail git -C super pull --recurse-submodules 2>actual &&
	test_i18ngrep "recurse-submodules is only valid with --rebase" actual
'

test_expect_success 'pull basic recurse' '
	(
		cd child &&
		echo "foobar" >>file &&
		git add file &&
		git commit -m "update file"
	) &&
	(
		cd parent &&
		git -C sub pull &&
		git add sub &&
		git commit -m "update submodule"
	) &&

	git -C parent rev-parse master >expect_super &&
	git -C child rev-parse master >expect_sub &&

	git -C super pull --rebase --recurse-submodules &&
	git -C super rev-parse master >actual_super &&
	git -C super/sub rev-parse master >actual_sub &&
	test_cmp expect_super actual_super &&
	test_cmp expect_sub actual_sub
'

test_expect_success 'pull basic rebase recurse' '
	(
		cd child &&
		echo "a" >file &&
		git add file &&
		git commit -m "update file"
	) &&
	(
		cd parent &&
		git -C sub pull &&
		git add sub &&
		git commit -m "update submodule"
	) &&
	(
		cd super/sub &&
		echo "b" >file2 &&
		git add file2 &&
		git commit -m "add file2"
	) &&

	git -C parent rev-parse master >expect_super &&
	git -C child rev-parse master >expect_sub &&

	git -C super pull --rebase --recurse-submodules &&
	git -C super rev-parse master >actual_super &&
	git -C super/sub rev-parse master^ >actual_sub &&
	test_cmp expect_super actual_super &&
	test_cmp expect_sub actual_sub &&

	echo "a" >expect &&
	test_cmp expect super/sub/file &&
	echo "b" >expect &&
	test_cmp expect super/sub/file2
'

test_expect_success 'pull rebase recursing fails with conflicts' '
	git -C super/sub reset --hard HEAD^^ &&
	git -C super reset --hard HEAD^ &&
	(
		cd super/sub &&
		echo "b" >file &&
		git add file &&
		git commit -m "update file"
	) &&
	test_must_fail git -C super pull --rebase --recurse-submodules
'

test_done
