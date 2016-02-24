#!/usr/bin/perl -w
#
#
# arguments:
#     perl packfsck.pl <target version>


my $target = $ARGV[0];
system("rm /tmp/Manifest");
system("wget --quiet --no-proxy --no-check-certificate --output-document=/tmp/Manifest https://download.clearlinux.org/update/$target/Manifest.os-core");


my $from = $target;
while ($from > $target - 100) {
 $from = $from - 10;
 print "Testing the $from-$target pack\n";
 system("rm /tmp/pack.tar");
 system("wget --quiet --no-proxy --no-check-certificate --output-document=/tmp/pack.tar https://download.clearlinux.org/update/$target/pack-os-core-from-$from.tar");



 open FILE, "</tmp/Manifest";

 my %expected_hashes;
 my %files;

 while (<FILE>) {
   my $line = $_;
  
  
   if ($line =~ /(^[FDLr\.]+)\s*([0-9a-f]+)\s*([0-9]+)\s*+(.*)\n/) {
      my $type = $1;
      my $hash = $2;
      my $version = $3;
      my $filename = $4;
      if ($version > $from) {
        $expected_hashes{"$hash"} = 1;
        $files{"$hash"} = $filename;
#        print "$1 - $2 - $3 - $4\n";
      }
   } 
 }
 close FILE;


 open FILE2, "-|", "tar -tf /tmp/pack.tar 2> /dev/null";

 while (<FILE2>) {
   my $line = $_;
  

  if ($line =~ /^\.\/delta\/[0-9]+.[0-9]+.([0-9a-f]+)/) {
      my $hash = $1;
      if (!defined($expected_hashes{"$hash"})) {
          print "\tUnexpected delta hash found $hash\n";
      }
      $expected_hashes{"$hash"} = 2;
  } 
  if ($line =~ /\.\/staged\/([0-9a-f]+)/) {
      my $hash = $1;
      if (!defined($expected_hashes{"$hash"})) {
          print "\tUnexpected staged hash found $hash\n";
      }
      $expected_hashes{"$hash"} = 2;
  }
 }
 close FILE2;

 my $count = 0;
 foreach my $key ( keys %expected_hashes )
 {
  my $value = $expected_hashes{$key};
  if ($value eq 1) {
     my $fn = $files{$key};
     print "\tFile $fn ($key) is not in the pack\n";
     $count = $count = 1;
  }
 }
 if ($count < 1) {
  print "\tThe $from-$target pack has no files missing\n";
 }
}
