#!/usr/bin/perl

use strict;
use warnings;

if(@ARGV<1){
  die"USAGE: $0 num_of_accounts num_of_threads num_of_txs\n";}

my $num=$ARGV[0];
my $thr=$ARGV[1];
my $txs=$ARGV[2];
my $skey="~/git/esc/ed25519/key";
my $user="~/git/esc/user";
my $file="/run/shm/in_$$";
my $node=0;
my %skey;
my $pkey;

open(FILE,">$file");
print FILE qq|{"run":"get_me"}\n|;
close(FILE);
open(FILE,"$user < $file 2>/dev/null |");
my @file=<FILE>;
close(FILE);
unlink($file);
#print join('',@file);
foreach(@file){
  if($_=~/"node": "(\d+)/){
    $node=sprintf("%04X",$1+0);
    next;}
  if($_=~/"public_key": "([A-F0-9]+)/){
    $pkey=$1;
    next;}
  if($_=~/"network_account"/){
    last;}}
if(!$node){
  die"ERROR, connection failed\n";}
print "NODE: $node\n";
print "PKEY: $pkey\n";

if($num<=1024){
  for(my $i=1;$i<$num;$i++){
    my $accn=sprintf("%4s-%08X-XXXX",$node,$i);
    open(FILE,"$skey $accn|");
    my @file=<FILE>;
    close(FILE);
    if(@file != 3){
    die"ERROR, unexpected $skey result\n".join("",@file);}
    $file[0]=~s/^SK: //; chomp($file[0]);
    #$file[1]=~s/^PK: //; chomp($file[1]);
    #$file[2]=~s/^SG: //; chomp($file[2]);
    $skey{$i}=$file[0];}}

open(FILE,">/tmp/get_me.json");
print FILE qq|{"run":"get_me"}\n|;
close(FILE);

my $pn=0;
while(1){
  if(!fork){
    my $anum=int(rand($num-1))+1;
    if($anum>=$num){
      $anum=$num-1;}
    my $accn=sprintf("%4s-%08X-XXXX",$node,$anum);
    my $uskey;
    if($num<=1024){
      $uskey=$skey{$anum};}
    else{
      open(FILE,"$skey $accn|");
      my @file=<FILE>;
      close(FILE);
      if(@file != 3){
      die"ERROR, unexpected $skey result\n".join("",@file);}
      $file[0]=~s/^SK: //; chomp($file[0]);
      #$file[1]=~s/^PK: //; chomp($file[1]);
      #$file[2]=~s/^SG: //; chomp($file[2]);
      $uskey=$file[0];}
    open(FILE,qq{$user --address "$accn" --secret "$uskey" < /tmp/get_me.json 2>/dev/null |});
    my @file=<FILE>;
    close(FILE);
    my $balance=0;
    foreach(@file){
      if($_=~/"balance": "([0-9\.]+)/){
        $balance=$1+0.0;
        last;}}
    if($balance<1.0){
      print "$accn $balance LOW!\n";
      sleep(5);
      exit;}
    print "$accn $balance\n";
    open(FILE,qq{|$user --address "$accn" --secret "$uskey" >/dev/null 2>&1});
    print FILE qq|{"run":"get_me"}\n|;
    for(my $i=0;$i<$txs;$i++){
      print FILE qq|{"run":"send_one","address":"0003-00000000-XXXX","amount":0.000002,"message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n|;}
    close(FILE);
    #sleep(1);
    exit;}
  if(++$pn>=$thr){
    wait();}}
while(wait()>=0){}
exit(0);

for(my $i=1;$i<$num;$i++){
  my $accn=sprintf("%4s-%08X-XXXX",$node,$i);
  open(FILE,">$file");
  print FILE qq|{"run":"get_account","address":"$accn"}\n|;
  close(FILE);
  open(FILE,"$user < $file 2>/dev/null |");
  my @file=<FILE>;
  close(FILE);
  my $upkey=""; 
  my $balance=0.0; 
  foreach(@file){
    if($_=~/"public_key": "([A-F0-9]+)/){
      $upkey=$1;
      next;}
    if($_=~/"balance": "([0-9\.]+)/){
      $balance=$1+0.0;
      next;}
    if($_=~/"network_account"/){
      last;}}
  if($upkey eq ""){
    print "CREATE $accn\n";
    open(FILE,">$file");
    print FILE qq|{"run":"get_me"}\n{"run":"create_account","node":"$node"}\n{"run":"send_one","address":"$accn","amount":"100.0","message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n{"run":"get_account","address":"$accn"}\n|;
    close(FILE);
    open(FILE,"$user < $file 2>/dev/null |");
    @file=<FILE>;
    close(FILE);
    $upkey=""; 
    $balance=0;
    my $uaccn="-----";
    foreach(@file){
      if($_=~/"address": "(....-........-)...."/){
        $uaccn=$1;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"public_key": "([A-F0-9]+)/){
        $upkey=$1;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"balance": "([0-9\.]+)/){
        $balance=$1+0.0;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"network_account"/){
        last;}}
    if($upkey eq ""){
      die"ERROR, failed to access account $accn\n".join("",@file);}}
  if($balance<1.0){
    print "SEND FUNDS to $accn\n";
    open(FILE,">$file");
    print FILE qq|{"run":"get_me"}\n{"run":"send_one","address":"$accn","amount":"100.0","message":"000102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F"}\n{"run":"get_account","address":"$accn"}\n|;
    close(FILE);
    open(FILE,"$user < $file 2>/dev/null |");
    @file=<FILE>;
    close(FILE);
    my $uaccn="-----";
    foreach(@file){
      if($_=~/"address": "(....-........-)...."/){
        $uaccn=$1;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"balance": "([0-9\.]+)/){
        $balance=$1+0.0;
        next;}
      if($accn=~/^$uaccn/ && $_=~/"network_account"/){
        last;}}
    if($balance<1.0){
      die"ERROR, failed to send funds to $accn\n".join("",@file);}}
  if($upkey eq $pkey){
    print "CHANGE KEY $accn\n";
    open(FILE,"$skey $accn|");
    @file=<FILE>;
    close(FILE);
    if(@file != 3){
      die"ERROR, unexpected $skey result\n".join("",@file);}
    $file[0]=~s/^SK: //; chomp($file[0]);
    $file[1]=~s/^PK: //; chomp($file[1]);
    $file[2]=~s/^SG: //; chomp($file[2]);
    $skey{$i}=$file[0];
    open(FILE,">$file");
    print FILE qq|{"run":"get_me"}\n{"run":"change_account_key","pkey":"$file[1]","signature":"$file[2]"}\n|;
    close(FILE);
    open(FILE,qq{$user --address "$accn" < $file 2>/dev/null |});
    #open(FILE,qq{$user --address "$accn" < $file |});
    my @fila=<FILE>;
    close(FILE);
    open(FILE,">$file");
    print FILE qq|{"run":"get_account","address":"$accn"}\n|;
    close(FILE);
    open(FILE,qq{$user < $file 2>/dev/null |});
    @file=<FILE>;
    close(FILE);
    $upkey=""; 
    foreach(@file){
      if($_=~/"public_key": "([A-F0-9]+)/){
        $upkey=$1;
        next;}
      if($_=~/"network_account"/){
        last;}}
    if($upkey eq ""){
      die"ERROR, failed to access account $accn\n".join("",@file);}
    if($upkey eq $pkey){
      die"ERROR, failed to change key $accn, try:\n".qq{$user < $file\n}.join("",@fila)."ME:\n".join("",@file);}}
  print sprintf("%04X PKEY: %s %.9f\n",$i,$upkey,$balance);}
unlink($file);