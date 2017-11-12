#!/bin/sh

test_description='Test default color/boolean/int/path'
. ./test-lib.sh

boolean()
{
	slot=$([ "$#" == 3 ] && echo $3 || echo "no.such.slot") &&
	actual=$(git config --default "$1" --bool "$slot") &&
	test "$actual" = "$2"
}

invalid_boolean()
{
	slot=$([ "$#" == 2 ] && echo $2 || echo "no.such.slot") &&
	test_must_fail git config --default "$1" --bool "$slot"
}

test_expect_success 'empty value for boolean' '
	invalid_boolean ""
'

test_expect_success 'true' '
	boolean "true" "true"
'

test_expect_success '1 is true' '
	boolean "1" "true"
'

test_expect_success 'non-zero is true' '
	boolean "5312" "true"
'

test_expect_success 'false' '
	boolean "false" "false"
'

test_expect_success '0 is false' '
	boolean "0" "false"
'

test_expect_success 'invalid value' '
	invalid_boolean "ab"
'

test_expect_success 'existing slot has priority = true' '
	git config bool.value true &&
	boolean "false" "true" "bool.value"
'

test_expect_success 'existing slot has priority = false' '
	git config bool.value false &&
	boolean "true" "false" "bool.value"
'

int()
{
	slot=$([ "$#" == 3 ] && echo $3 || echo "no.such.slot") &&
	actual=$(git config --default "$1" --int "$slot") &&
	test "$actual" = "$2"
}

invalid_int()
{
	slot=$([ "$#" == 2 ] && echo $2 || echo "no.such.slot") &&
	test_must_fail git config "$1" --int "$slot"
}

test_expect_success 'empty value for int' '
	invalid_int "" ""
'

test_expect_success 'positive' '
	int "12345" "12345"
'

test_expect_success 'negative' '
	int "-679032" "-679032"
'

test_expect_success 'invalid value' '
	invalid_int "abc"
'
test_expect_success 'existing slot has priority = 123' '
	git config int.value 123 &&
	int "666" "123" "int.value"
'

test_expect_success 'existing slot with bad value' '
	git config int.value abc &&
	invalid_int "123" "int.value"
'

path()
{
	slot=$([ "$#" == 3 ] && echo $3 || echo "no.such.slot") &&
	actual=$(git config --default "$1" --path "$slot") &&
	test "$actual" = "$2"
}

invalid_path()
{
	slot=$([ "$#" == 2 ] && echo $2 || echo "no.such.slot") &&
	test_must_fail git config "$1" --path "$slot"
}

test_expect_success 'empty path is invalid' '
	invalid_path "" ""
'

test_expect_success 'valid path' '
	path "/aa/bb/cc" "/aa/bb/cc"
'

test_expect_success 'existing slot has priority = /to/the/moon' '
	git config path.value /to/the/moon &&
	path "/to/the/sun" "/to/the/moon" "path.value"
'
ESC=$(printf '\033')

color()
{
	slot=$([ "$#" == 3 ] && echo $3 || echo "no.such.slot") &&
	actual=$(git config --default "$1" --color "$slot" ) &&
	test "$actual" = "${2:+$ESC}$2"
}

invalid_color()
{
	slot=$([ "$#" == 2 ] && echo $2 || echo "no.such.slot") &&
	test_must_fail git config --default "$1" --color "$slot"
}

test_expect_success 'reset' '
	color "reset" "[m"
'

test_expect_success 'empty color is empty' '
	color "" ""
'

test_expect_success 'attribute before color name' '
	color "bold red" "[1;31m"
'

test_expect_success 'color name before attribute' '
	color "red bold" "[1;31m"
'

test_expect_success 'attr fg bg' '
	color "ul blue red" "[4;34;41m"
'

test_expect_success 'fg attr bg' '
	color "blue ul red" "[4;34;41m"
'

test_expect_success 'fg bg attr' '
	color "blue red ul" "[4;34;41m"
'

test_expect_success 'fg bg attr...' '
	color "blue bold dim ul blink reverse" "[1;2;4;5;7;34m"
'

# note that nobold and nodim are the same code (22)
test_expect_success 'attr negation' '
	color "nobold nodim noul noblink noreverse" "[22;24;25;27m"
'

test_expect_success '"no-" variant of negation' '
	color "no-bold no-blink" "[22;25m"
'

test_expect_success 'long color specification' '
	color "254 255 bold dim ul blink reverse" "[1;2;4;5;7;38;5;254;48;5;255m"
'

test_expect_success 'absurdly long color specification' '
	color \
	  "#ffffff #ffffff bold nobold dim nodim italic noitalic
	   ul noul blink noblink reverse noreverse strike nostrike" \
	  "[1;2;3;4;5;7;9;22;23;24;25;27;29;38;2;255;255;255;48;2;255;255;255m"
'

test_expect_success '0-7 are aliases for basic ANSI color names' '
	color "0 7" "[30;47m"
'

test_expect_success '256 colors' '
	color "254 bold 255" "[1;38;5;254;48;5;255m"
'

test_expect_success '24-bit colors' '
	color "#ff00ff black" "[38;2;255;0;255;40m"
'

test_expect_success '"normal" yields no color at all"' '
	color "normal black" "[40m"
'

test_expect_success '-1 is a synonym for "normal"' '
	color "-1 black" "[40m"
'

test_expect_success 'color too small' '
	invalid_color "-2"
'

test_expect_success 'color too big' '
	invalid_color "256"
'

test_expect_success 'extra character after color number' '
	invalid_color "3X"
'

test_expect_success 'extra character after color name' '
	invalid_color "redX"
'

test_expect_success 'extra character after attribute' '
	invalid_color "dimX"
'

test_expect_success 'existing slot has priority = red' '
	git config color.value red &&
	path "blue" "red" "color.value"
'

test_done
