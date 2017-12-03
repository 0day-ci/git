package Git::Error;
use 5.008;
use strict;
use warnings;

=head1 NAME

Git::Error - Wrapper for the L<Error> module, in case it's not installed

=head1 DESCRIPTION

Wraps the import function for the L<Error> module. The L<Git> module
must be imported first.

This module is only intended to be used for code shipping in the
C<git.git> repository. Use it for anything else at your peril!

=cut

sub import {
    shift;
    my $caller = caller;

    eval {
	require Error;
	1;
    } or do {
	my $error = $@ || "Zombie Error";

	my $Git_pm_path = $INC{"Git.pm"} || die "BUG: Should have loaded the Git module first!";

	require File::Basename;
	my $Git_pm_root = File::Basename::dirname($Git_pm_path) || die "BUG: Can't figure out dirname from '$Git_pm_path'!";

	require File::Spec;
	my $Git_pm_FromCPAN_root = File::Spec->catdir($Git_pm_root, qw(Git FromCPAN));
	die "BUG: '$Git_pm_FromCPAN_root' should be a directory!" unless -d $Git_pm_FromCPAN_root;

	local @INC = ($Git_pm_FromCPAN_root, @INC);
	require Error;
    };

    local @_ = ($caller, @_);
    goto &Error::import;
}

1;
