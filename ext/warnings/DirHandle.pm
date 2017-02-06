package DirHandle;

our $VERSION = '1.05';

=head1 NAME 

DirHandle - supply object methods for directory handles

=head1 SYNOPSIS

    use DirHandle;
    $d = DirHandle->new(".");
    if (defined $d) {
        while (defined($_ = $d->read)) { something($_); }
        $d->rewind;
        while (defined($_ = $d->read)) { something_else($_); }
        undef $d;
    }

=head1 DESCRIPTION

The C<DirHandle> method provide an alternative interface to the
opendir(), closedir(), readdir(), and rewinddir() functions.

The only objective benefit to using C<DirHandle> is that it avoids
namespace pollution by creating globs to hold directory handles.

=cut

require 5.000;
use Symbol;

sub new ($class, $dirname?) {
    my $dh = gensym;
    if (defined $dirname) {
	DirHandle::open($dh, $dirname)
	    or return undef;
    }
    bless $dh, $class;
}

sub DESTROY ($dh) {
    # Don't warn about already being closed as it may have been closed 
    # correctly, or maybe never opened at all.
    local($., $@, $!, $^E, $?);
    no warnings 'io';
    closedir($dh);
}

sub open($dh, str $dirname) {
    opendir($dh, $dirname);
}

sub close ($dh) {
    closedir($dh);
}

sub read ($dh) {
    readdir($dh);
}

sub rewind ($dh) {
    rewinddir($dh);
}

1;
