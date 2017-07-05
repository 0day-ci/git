#!/bin/sh

test_description='various tests of reflog walk (log -g) behavior'
. ./test-lib.sh

test_expect_success 'set up some reflog entries' '
	test_commit one &&
	test_commit two &&
	git checkout -b side HEAD^ &&
	test_commit three &&
	git merge --no-commit master &&
	echo evil-merge-content >>one.t &&
	test_tick &&
	git commit --no-edit -a
'

do_walk () {
	git log -g --format="%gd %gs" "$@"
}

sq="'"
test_expect_success 'set up expected reflog' '
	cat >expect.all <<-EOF
	HEAD@{0} commit (merge): Merge branch ${sq}master${sq} into side
	HEAD@{1} commit: three
	HEAD@{2} checkout: moving from master to side
	HEAD@{3} commit: two
	HEAD@{4} commit (initial): one
	EOF
'

test_expect_success 'reflog walk shows expected logs' '
	do_walk >actual &&
	test_cmp expect.all actual
'

test_expect_success 'reflog can limit with --no-merges' '
	grep -v merge expect.all >expect &&
	do_walk --no-merges >actual &&
	test_cmp expect actual
'

test_expect_success 'reflog can limit with pathspecs' '
	grep two expect.all >expect &&
	do_walk -- two.t >actual &&
	test_cmp expect actual
'

test_expect_success 'pathspec limiting handles merges' '
	sed -n "1p;3p;5p" expect.all >expect &&
	do_walk -- one.t >actual &&
	test_cmp expect actual
'

test_expect_success '--parents shows true parents' '
	# convert newlines to spaces
	echo $(git rev-parse HEAD HEAD^1 HEAD^2) >expect &&
	git rev-list -g --parents -1 HEAD >actual &&
	test_cmp expect actual
'

test_expect_success 'walking multiple reflogs shows both' '
	{
		do_walk HEAD &&
		do_walk side
	} >expect &&
	do_walk HEAD side >actual &&
	test_cmp expect actual
'

test_expect_success 'walk prefers reflog to ref tip' '
	head=$(git rev-parse HEAD) &&
	one=$(git rev-parse one) &&
	ident="$GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE" &&
	echo "$head $one $ident	broken reflog entry" >>.git/logs/HEAD &&

	echo $one >expect &&
	git log -g --format=%H -1 >actual &&
	test_cmp expect actual
'

test_expect_success 'rev-list -g complains when there are no reflogs' '
	test_must_fail git rev-list -g
'

test_done
