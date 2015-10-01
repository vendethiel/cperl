# miniperl compatible boot stage dummy, to be able to compile the real version
package warnings;
our $VERSION = '2.00';
sub import { $^W = 0; }
sub warnif { }
sub register_categories { }
sub enabled { 0 }
sub _chk { 0 }
