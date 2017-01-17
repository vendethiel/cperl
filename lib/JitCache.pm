package JitCache;
our $VERSION = "0.1";

sub import {
  my ($module, $file, @subs) = @_;
  # perl5 fakes .pm filename for a .pmc, cperl5.25.3 not anymore
  die "Not a .p[lm]c $file" unless $file =~ /^(.*)\.p[lm]c$/;
  my $bc = $1.".bc";
  $file =~ s/c$//;    # must ignore the PMC! cperl-5.25.3 only
  my $ret = do $file; # perl5 will also fail here
  # get the parallel .bc file
  if (-e $bc and -s _ and -M _ < -M $file) {
    load($bc); # XS
  }
  return $ret;
}

#sub save(str $file, @subs) {
#}
