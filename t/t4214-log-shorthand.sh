#!/bin/sh

test_description='log can show previous branch using shorthand - for @{-1}'

. ./test-lib.sh

test_expect_success 'setup' '
	echo hello >world &&
	git add world &&
	git commit -m initial &&
	echo "hello second time" >>world &&
	git add world &&
	git commit -m second &&
	echo "hello other file" >>planet &&
	git add planet &&
	git commit -m third &&
	echo "hello yet another file" >>city &&
	git add city &&
	git commit -m fourth
'

test_expect_success '"log -" should not work initially' '
	test_must_fail git log -
'

test_expect_success '"log -" should work' '
	git checkout -b testing-1 master^ &&
	git checkout -b testing-2 master~2 &&
	git checkout master &&

	git log testing-2 >expect &&
	git log - >actual &&
	test_cmp expect actual
'

test_expect_success 'symmetric revision range should work when one end is left empty' '
	git checkout testing-2 &&
	git checkout master &&
	git log ...@{-1} > expect.first_empty &&
	git log @{-1}... > expect.last_empty &&
	git log ...- > actual.first_empty &&
	git log -... > actual.last_empty &&
	test_cmp expect.first_empty actual.first_empty &&
	test_cmp expect.last_empty actual.last_empty
'

test_expect_success 'asymmetric revision range should work when one end is left empty' '
	git checkout testing-2 &&
	git checkout master &&
	git log ..@{-1} > expect.first_empty &&
	git log @{-1}.. > expect.last_empty &&
	git log ..- > actual.first_empty &&
	git log -.. > actual.last_empty &&
	test_cmp expect.first_empty actual.first_empty &&
	test_cmp expect.last_empty actual.last_empty
'

test_expect_success 'symmetric revision range should work when both ends are given' '
	git checkout testing-2 &&
	git checkout master &&
	git log -...testing-1 >expect &&
	git log testing-2...testing-1 >actual &&
	test_cmp expect actual
'

test_expect_success 'asymmetric revision range should work when both ends are given' '
	git checkout testing-2 &&
	git checkout master &&
	git log -..testing-1 >expect &&
	git log testing-2..testing-1 >actual &&
	test_cmp expect actual
'
test_done
