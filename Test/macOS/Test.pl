#!/usr/bin/perl -w
use strict;
use warnings;


my $indentChar = " ";
my $indent = "    ";
$indentChar = "\t";
$indent = "\t";
my $indents = "[$indentChar]*";
my $repeats = 0;
my $missing = "";

$_ = '
(
	const_8fa_0 \ (8fa 0) \ [8fa] 
	unnamed_fcode_8dc \ [8dc] 
	field_8dc_2 \ (8dc 2) \ [8dc] 
	unnamed_fcode_9c7 \ [9c7] 
	unnamed_fcode_8dc \ [8dc] 
	unnamed_fcode_9c7 \ [9c7] 
	colon_definition_function_9c7 \ (9c7) \ [9c7] 
	unnamed_fcode_9c7 \ [9c7] 
	unnamed_fcode_9c7 \ [9c7] 
	unnamed_fcode_8dc \ [8dc] 
	00000
	00000
	if \ (0x20) 
		if \ (0x22) 
			again \ (0xffffff22) 
		then 
		bbbb
		bbbb
		again \ (0xffffff11) 
	then 
	00000
	if \ (0x20) 
		aaaaa
		bbbb
		bbbb
		if \ (0x22) 
			ccccc
			ccccc
			again \ (0xffffff22) 
		then 
		bbbb
		bbbb
		again \ (0xffffff11) 
	then 
	b(") \ [0x012] " 2149" 
	aaaa
	aaa
)
';



print $_;

if (0) {
	while(/\G.*?(a[^a]+a)/gs) {
		print $-[1].", ".$+[1].", '".$1."'\n";
	}
}
elsif (0) {
	s/a[^a]+a/bbb/mgs;
	print $_;
}
elsif (0) {
	#         1           2                                             3                                      4     5                                             
	while (/^(${indents})(if) \\ \(0x[0-9a-f]{1,4}\) (?:\\ \[0x014\] )?(\n)(?:\1${indent}[^\n]*\n)*\1${indent}(again( \\ \(0xffff[0-9a-f]{4}\) (?:\\ \[0x013\] )?)\n\1then )/mg) {
		substr($_, $-[4], $+[4] - $-[4], "repeat$5");
		substr($_, $-[2], $+[2] - $-[2], "while");
		pos = $-[3] + 3; # while is 3 characters longer than if
		print $_;
		$repeats++;
	}
	print "done".$repeats;
}
elsif (0) {
	while (/^(${indents}.*(\n).*)\n/mg) {
		print "{".$1."} at ".pos."\n";
		pos = $+[2];
	}
}
elsif (1) {
	my @fcodes;
	while (/\cl([^\cl]+)\cl(([0-9a-f]{3})[^\cl]*)\cl([^\cl]*)\cl[^\n]*(\n)/g) { push @fcodes, [$+[4], $1.$2.$4, $3] }
	for my $fcode (reverse @fcodes) {
		print @$fcode[0].",".@$fcode[1].",".@$fcode[2]."\n";
		pos = @$fcode[0];
		while ( /\G.*?\b(unnamed_fcode_@$fcode[2]) [^\n]*(\n)/mgs ) {
			substr($_, $-[1], $+[1] - $-[1], @$fcode[1]);
			pos = $+[2];
		}
	}
	s/^(${indents})b\(\"\) ((?:\\ \[0x012\] )?)(.*)/$1$3$2/mg;



#===================================================
# Do formatting

# convert indent tabs to spaces
while (/^(\t+)/mg) {
	my $start = $-[1];
	my $length = $+[1] - $-[1];
	my $replacement = $1;
	$replacement =~ s/\t/    /g;
	substr($_, $start, $length, $replacement);
	pos = $start + length($replacement);
}


# protect slash definition
s/\\ \\/\ck \\/mg || do { $missing .= "\nslash definition" };

my $_s = '                                                                                '; # lots of spaces
while (/^(.*?)(\\[^["]*)([][()\\ 0-9a-fx-]*)$/mg) {
	my $start = $-[1];
	my $length = $+[3] - $-[1];

	# divide the line into the line part, comment part and instruction part.
	my $_l = $1; # line
	my $_c = $2; # comment
	my $_i = $3; # instruction

	# clean up line (remove trailing spaces)
	$_l =~ s/[ ]+$//;

	# clean up comment (remove trailing backslash and spaces)
	$_c =~ s/ \\ $//;
	$_c =~ s/[ ]+$//;

	#clean up the instructions (remove 0x and inner brackets)
	$_i =~ s/] \\ \[0x/ /g;
	$_i =~ s/\[0x/[/;

	# calculate tabs between comment and instructions
	my $_c2 = $_c.$_s;
	$_c2 =~ s/^(.{19} ) *$/$1/;			# comment that fits
	$_c2 =~ s/^(.{19}.*[^ ] ) *$/$1/;	# comment that doesn't fit
	$_c2 =~ s/^.*[^ ]( +)$/$1/;			# just grab the spaces
	$_c2 =~ s/    /\t/g;				# convert to tabs
	$_c2 =~ s/ +/\t/;					# convert remaining spaces to a tab

	# calculate tabs between the line and comment
	my $_l2 = $_l.$_s;
	$_l2 =~ s/^(.{79} ) *$/$1/;			# line that fits
	$_l2 =~ s/^(.{79}.*[^ ] ) *$/$1/;	# line that doesn't fits
	$_l2 =~ s/^.*[^ ]( +)$/$1/;			# just grab the spaces
	$_l2 =~ s/    /\t/g;				# convert to tabs
	$_l2 =~ s/ +/\t/;					# convert remaining spaces to a tab
	
	my $replacement = $_l.$_l2.$_c.$_c2.$_i;
	substr($_, $start, $length, $replacement);
	pos = $start + length($replacement) + 1;
}

# restore slash definition
s/\ck/\\/g;

# convert indent spaces to tabs
while (/^((?:    )+)/mg) {
	my $start = $-[1];
	my $length = $+[1] - $-[1];
	my $replacement = $1;
	$replacement =~ s/    /\t/g;
	substr($_, $start, $length, $replacement);
	pos = $start + length($replacement);
}

# remove trailing spaces and tabs
s/[ \t]+$//mg;


#===================================================


	print $_;
}
