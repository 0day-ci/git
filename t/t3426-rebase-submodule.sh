#!/bin/sh

test_description='rebase can handle submodules'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-submodule-update.sh
. "$TEST_DIRECTORY"/lib-rebase.sh

git_rebase () {
	git status -su >expect &&
	ls -1pR * >>expect &&
	git checkout -b ours HEAD &&
	echo x >>file1 &&
	git add file1 &&
	git commit -m add_x &&
	git revert HEAD &&
	git status -su >actual &&
	ls -1pR * >>actual &&
	test_cmp expect actual &&
	git rebase "$1"
}

test_submodule_switch "git_rebase"

git_rebase_interactive () {
	git status -su >expect &&
	ls -1pR * >>expect &&
	git checkout -b ours HEAD &&
	echo x >>file1 &&
	git add file1 &&
	git commit -m add_x &&
	git revert HEAD &&
	git status -su >actual &&
	ls -1pR * >>actual &&
	test_cmp expect actual &&
	set_fake_editor &&
	echo "fake-editor.sh" >.git/info/exclude &&
	git rebase -i "$1"
}

test_submodule_switch "git_rebase_interactive"

test_expect_success 'rebase interactive ignores modified submodules' '
	test_when_finished "rm -rf super sub" &&
	git init sub &&
	git -C sub commit --allow-empty -m "Initial commit" &&
	git init super &&
	git -C super submodule add ../sub &&
	git -C super config submodule.sub.ignore dirty &&
	> super/foo &&
	git -C super add foo &&
	git -C super commit -m "Initial commit" &&
	test_commit -C super a &&
	test_commit -C super b &&
	test_commit -C super/sub c &&
	git -C super rebase -i HEAD^^
'

test_done
