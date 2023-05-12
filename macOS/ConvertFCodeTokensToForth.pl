#!/usr/bin/perl -w
use strict;
use warnings;
use Time::Piece;

my $time1 = localtime();

sub timeit {
	my $name = $_[0];
	my $time2 = localtime();
	print STDERR "# Time: " . ($time2 - $time1) . " : " . $name . "\n";
	$time1 = $time2;
}


print STDERR "========================\nConvertFCodeTokensToForth.pl log:\n";

my ($fcodeInput, $pciHeaderFile) = @ARGV;

if (not defined $fcodeInput) {
	die "Need fcodeInput\n";
}

if (not defined $pciHeaderFile) {  # header info
	$pciHeaderFile = "";
}

$_ = "";

{
	local $/=undef;
	open FILE, $fcodeInput or die "Couldn't open file: $fcodeInput";
	$_ = <FILE>;
	close FILE;
}

#90123
#ijklm
if (/[\cl\ck]/) {
	print STDERR "# File contains ctrl-l or ctrl-k characters.\n";
	exit 1; # make sure our special escape character (ctrl-k) doesn't exist}
}

# figure out what the indent character is

# assume indent character is tab
my $indentChar = " ";
my $indent = "    ";

# perl performance note: (?<=\n) is sometimes faster than ^

# if exists a line that starts with a space character, then indent character is space, and a full indent is four spaces
if (/(?<=\n)[\t]/m) {
	$indentChar = "\t";
	$indent = "\t";
	#print STDERR "# Indent is tab character.\n";
} else {
	#print STDERR "# Indent is space character.\n";
}

my $indents = "[$indentChar]*+"; # ${indents}
my $indentsnotgreedy = "[$indentChar]*"; # ${indentsnotgreedy}
my $number = "-?\\d[0-9a-f]*|\\/[cwl]|false|true|d#\\d+|h#[\\da-f]+"; # ${number}
my $fcodep = "[0-9a-f]{3,4}"; # ${fcodep} # Power Mac Quad G5 allows 4 digit fcodes

# pci header

if ($pciHeaderFile ne "") {
	local $/=undef;
	open FILE, $pciHeaderFile or die "Couldn't open file: $fcodeInput";
	my $thePCIHeader = <FILE>;
	close FILE;
	substr($_, 0, 0, "$thePCIHeader");
} timeit("pciheader");

# fcode-version2
my $gotstart = 0;
  s/start1 ((?:\\ \[0x0f1\] )?)\n  (format:    0x08)\n  (checksum:  0x[\da-f]+ \(.*?\))\n  (len:       0x[\da-f]+ \(\d+ bytes\))/fcode-version2 $1\n\\ $2\n\\ $3\n\\ $4\nhex\n\n/g && do { $gotstart = 1 }; timeit("gotstart1");
s/version1 ((?:\\ \[0x0fd\] )?)\n  (format:    0x08)\n  (checksum:  0x[\da-f]+ \(.*?\))\n  (len:       0x[\da-f]+ \(\d+ bytes\))/fcode-version1 $1\n\\ $2\n\\ $3\n\\ $4\nhex\n\n/g && do { $gotstart = 1 }; timeit("version1");

if ($gotstart == 0) {
	print STDERR "# Didn't get start fcode.\n";
}

# fcode-end, pci-end
my $gotend = 0;
if ( s/(?<=\n)(${indents})(?:end0|b\(end0\)) (?:\\ \[0x000\] )?\n(\\ detokenizing finished after .*?bytes\.\n)/\n$1fcode-end\n$2/mg ) { $gotend = 1 }; timeit("gotend");

if ($pciHeaderFile ne "") {
	$_ .= "pci-end\n";
}

if ($gotend == 0) {
	print STDERR "# Didn't get end fcode.\n";
	#exit 1
}

my $missing = "";


# first do literal numbers so they can be used in token names

s/(?<=\n)(${indents})b\(lit\) ((?:\\ \[0x010\] )?)0x(?:ffffffff)?ffffffff\b/${1}-1 $2/mg || do { $missing .= "\nb(lit) ffffffff" }; timeit("b(lit) ffffffff -> -1");
s/(?<=\n)(${indents})b\(lit\) ((?:\\ \[0x010\] )?)0x(?:ffffffff)?([a-f][\da-f]{7})\b/${1}0$3 $2/mg || do { $missing .= "\nb(lit) a-fxxxxxxx" }; timeit("b(lit) negative number needs a leading zero");
s/(?<=\n)(${indents})b\(lit\) ((?:\\ \[0x010\] )?)0x(?:ffffffff)?([89][\da-f]{7})\b/${1}$3 $2/mg || do { $missing .= "\nb(lit) 8-9xxxxxxx" }; timeit("b(lit) negative number");
s/(?<=\n)(${indents})b\(lit\) ((?:\\ \[0x010\] )?)0x([a-f][\da-f]*)/${1}0$3 $2/mg || do { $missing .= "\nb(lit) a-f..." }; timeit("b(lit) a-f...");
s/(?<=\n)(${indents})b\(lit\) ((?:\\ \[0x010\] )?)0x(\d[0-9a-f]*)/${1}$3 $2/mg || do { $missing .= "\nb(lit) \\d..." }; timeit("b(lit) \\d...");

# next do tokens

#	external        Newly created functions will be visible.                Arrange for subsequently created FCode functions to use external-token
#	headers         Newly created functions will be optionally visible.     Arrange for subsequently created FCode functions to use named-token
#	headerless      Newly created functions will be invisible.              Arrange for subsequently created FCode functions to use new-token
#	instance        value, variable, defer, or buffer:                      Used as: 30 instance value new-name
#	vectored        value, variable, defer, or buffer:                      Used as: 30 vectored value new-name # Power Mac Quad G5
#
#	new-token       (Tokenized by defining words in headerless mode)        new-token fcode
#	named-token     (Tokenized by defining words in headers mode)           named-token name fcode
#	external-token  (Tokenized by defining words in external mode)          external-token name fcode
#
#
#	buffer:         new-token|named-token|external-token b(buffer:)         num <instance> buffer: name
#	value			new-token|named-token|external-token b(value)           num <instance> value name
#
#	defer			new-token|named-token|external-token b(defer)           <instance> defer name
#	variable		new-token|named-token|external-token b(variable)        <instance> variable name
#
#	constant        new-token|named-token|external-token b(constant)        num constant name
#	field           new-token|named-token|external-token b(field)           num field name
#
#	:               new-token|named-token|external-token b(:)				: name
#	create          new-token|named-token|external-token b(create)          create name
#	code            new-token|named-token|external-token b(code)            code name


my @words = (
	"num <instance> buffer: name buffer",
	"num <instance> value name value",
	"<instance> defer name defer_word_function",
	"<instance> variable name variable",
	"num constant name const",
	"num field name field",
	": name colon_definition_function",
	"create name create_word_function",
	"code name code_function"
);


foreach my $theword (@words) {
	if ($theword =~ /((?:num )*)((?:<instance> )*)(.*?) name (.*)/) {

		my $numpart = $1;
		my $instancepart = $2;
		my $fcodetype = $3;
		my $unnameprefix = $4;
		
		if ($instancepart) {
			if ($numpart) {
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n(?<r4>(?:${indents}instance (?<r8>(\\ \[0x0c0\] )?)\n)*)${indents}new-token (?<r7>(\\ \[0x${fcodep}\] )?)0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknew\ck$+{r1}$+{r2} $fcodetype \cl${unnameprefix}_\cl$+{r3}_$+{r2}\cl \\ \($+{r3} $+{r2}\)\cl $+{r6}\ck$+{r7}/mg || do { $missing .= "\ninstance \cknew\ck $numpart$fcodetype" }; timeit($unnameprefix . " num instance new" );
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n(?<r4>(?:${indents}instance (?<r8>(\\ \[0x0c0\] )?)\n)*)${indents}named-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknamed\ck$+{r1}$+{r2} $fcodetype $+{r5} \\ \($+{r3} $+{r2}\) $+{r6}\ck$+{r7}/mg || do { $missing .= "\ninstance \cknamed\ck $numpart$fcodetype" }; timeit($unnameprefix . " num instance named" );
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n(?<r4>(?:${indents}instance (?<r8>(\\ \[0x0c0\] )?)\n)*)${indents}external-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\ckexternal\ck$+{r1}$+{r2} $fcodetype $+{r5} \\ \($+{r3} $+{r2}\) $+{r6}\ck$+{r7}/mg || do { $missing .= "\ninstance \ckexternal\ck $numpart$fcodetype" }; timeit($unnameprefix . " num instance external" );
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n(?<r4>(?:${indents}vectored (?<r8>(\\ \[0x${fcodep}\] )?)\n)*)${indents}new-token (?<r7>(\\ \[0x${fcodep}\] )?)0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknew\ck$+{r1}$+{r2} $fcodetype \cl${unnameprefix}_\cl$+{r3}_$+{r2}\cl \\ \($+{r3} $+{r2}\)\cl $+{r6}\ck$+{r7}/mg || do { $missing .= "\nvectored \cknew\ck $numpart$fcodetype" }; timeit($unnameprefix . " num vectored new" );
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n(?<r4>(?:${indents}vectored (?<r8>(\\ \[0x${fcodep}\] )?)\n)*)${indents}named-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknamed\ck$+{r1}$+{r2} $fcodetype $+{r5} \\ \($+{r3} $+{r2}\) $+{r6}\ck$+{r7}/mg || do { $missing .= "\nvectored \cknamed\ck $numpart$fcodetype" }; timeit($unnameprefix . " num vectored named" );
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n(?<r4>(?:${indents}vectored (?<r8>(\\ \[0x${fcodep}\] )?)\n)*)${indents}external-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\ckexternal\ck$+{r1}$+{r2} $fcodetype $+{r5} \\ \($+{r3} $+{r2}\) $+{r6}\ck$+{r7}/mg || do { $missing .= "\nvectored \ckexternal\ck $numpart$fcodetype" }; timeit($unnameprefix . " num vectored external" );
			}
			s/(?<=\n)(?<r1>${indents})(?<r4>(?:${indents}instance (?<r8>(\\ \[0x0c0\] )?)\n)*)${indents}new-token (?<r7>(\\ \[0x${fcodep}\] )?)0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknew\ck$+{r1}$fcodetype \cl${unnameprefix}_\cl$+{r3}\cl \\ \($+{r3}\)\cl \ck$+{r7}/mg || do { $missing .= "\ninstance \cknew\ck $fcodetype" }; timeit($unnameprefix . " instance new" );
			s/(?<=\n)(?<r1>${indents})(?<r4>(?:${indents}instance (?<r8>(\\ \[0x0c0\] )?)\n)*)${indents}named-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknamed\ck$+{r1}$fcodetype $+{r5} \\ \($+{r3}\) \ck$+{r7}/mg || do { $missing .= "\ninstance \cknamed\ck $fcodetype" }; timeit($unnameprefix . " instance named" );
			s/(?<=\n)(?<r1>${indents})(?<r4>(?:${indents}instance (?<r8>(\\ \[0x0c0\] )?)\n)*)${indents}external-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\ckexternal\ck$+{r1}$fcodetype $+{r5} \\ \($+{r3}\) \ck$+{r7}/mg || do { $missing .= "\ninstance \ckexternal\ck $fcodetype" }; timeit($unnameprefix . " instance external" );
			s/(?<=\n)(?<r1>${indents})(?<r4>(?:${indents}vectored (?<r8>(\\ \[0x${fcodep}\] )?)\n)*)${indents}new-token (?<r7>(\\ \[0x${fcodep}\] )?)0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknew\ck$+{r1}$fcodetype \cl${unnameprefix}_\cl$+{r3}\cl \\ \($+{r3}\)\cl \ck$+{r7}/mg || do { $missing .= "\nvectored \cknew\ck $fcodetype" }; timeit($unnameprefix . " vectored new" );
			s/(?<=\n)(?<r1>${indents})(?<r4>(?:${indents}vectored (?<r8>(\\ \[0x${fcodep}\] )?)\n)*)${indents}named-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\cknamed\ck$+{r1}$fcodetype $+{r5} \\ \($+{r3}\) \ck$+{r7}/mg || do { $missing .= "\nvectored \cknamed\ck $fcodetype" }; timeit($unnameprefix . " vectored named" );
			s/(?<=\n)(?<r1>${indents})(?<r4>(?:${indents}vectored (?<r8>(\\ \[0x${fcodep}\] )?)\n)*)${indents}external-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /$+{r4}\ckexternal\ck$+{r1}$fcodetype $+{r5} \\ \($+{r3}\) \ck$+{r7}/mg || do { $missing .= "\nvectored \ckexternal\ck $fcodetype" }; timeit($unnameprefix . " vectored external" );
		}
		else {
			if ($numpart) {
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n${indents}new-token (?<r7>(\\ \[0x${fcodep}\] )?)0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /\cknew\ck$+{r1}$+{r2} $fcodetype \cl${unnameprefix}_\cl$+{r3}_$+{r2}\cl \\ \($+{r3} $+{r2}\)\cl $+{r6}\ck$+{r7}/mg || do { $missing .= "\n\cknew\ck $numpart$fcodetype" }; timeit($unnameprefix . " num new" );
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n${indents}named-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /\cknamed\ck$+{r1}$+{r2} $fcodetype $+{r5} \\ \($+{r3} $+{r2}\) $+{r6}\ck$+{r7}/mg || do { $missing .= "\n\cknamed\ck $numpart$fcodetype" }; timeit($unnameprefix . " num named" );
				s/(?<=\n)(?<r1>${indents})(?<r2>${number}) (?<r6>(?:\\ \[0x${fcodep}\] )?)\n${indents}external-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /\ckexternal\ck$+{r1}$+{r2} $fcodetype $+{r5} \\ \($+{r3} $+{r2}\) $+{r6}\ck$+{r7}/mg || do { $missing .= "\n\ckexternal\ck $numpart$fcodetype" }; timeit($unnameprefix . " num external" );
			}
			s/(?<=\n)(?<r1>[$indentChar]*)[$indentChar]*new-token (?<r7>(\\ \[0x${fcodep}\] )?)0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /\cknew\ck$+{r1}$fcodetype \cl${unnameprefix}_\cl$+{r3}\cl \\ \($+{r3}\)\cl \ck$+{r7}/mg || do { $missing .= "\n\cknew\ck $fcodetype" }; timeit($unnameprefix . " new" );
			s/(?<=\n)(?<r1>[$indentChar]*)[$indentChar]*named-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /\cknamed\ck$+{r1}$fcodetype $+{r5} \\ \($+{r3}\) \ck$+{r7}/mg || do { $missing .= "\n\cknamed\ck $fcodetype" }; timeit($unnameprefix . " named" );
			s/(?<=\n)(?<r1>[$indentChar]*)[$indentChar]*external-token (?<r7>(\\ \[0x${fcodep}\] )?)(?<r5>[^ ]+) 0x(?<r3>[\da-f]+)\n${indents}b\($fcodetype\) /\ckexternal\ck$+{r1}$fcodetype $+{r5} \\ \($+{r3}\) \ck$+{r7}/mg || do { $missing .= "\n\ckexternal\ck $fcodetype" }; timeit($unnameprefix . " external" );
		}
	} else {
		exit 1;
	}
}

      s/(?<=\n)${indents}instance ((?:\\ \[0x0c0] )?)\n(\ck[^\ck\n]+\ck${indents}(?:(?:${number}) )?)([^\ck\n]+)/${2}instance $3$1/mg || do { $missing .= "\ninstance" }; timeit("move instance to between num and forth word");
s/(?<=\n)${indents}vectored ((?:\\ \[0x${fcodep}] )?)\n(\ck[^\ck\n]+\ck${indents}(?:(?:${number}) )?)([^\ck\n]+)/${2}vectored $3$1/mg || do { $missing .= "\nvectored" }; timeit("move vectored to between num and forth word");
s/(?<=\n)(\ck[^\ck\n]+\ck(${indents})(?:instance |vectored )?: )/$2\n$1/mg || do { $missing .= "\n:" }; timeit("place a carriage return before :");
s/(?<=\n)(${indents})0 ((?:\\ \[0x${fcodep}\] )*)\n(\ck[^\ck\n]+\ck)(${indents}(?:[\da-f]+|\/[cwl]) field )/$3$1struct $2\n$4/mg || do { $missing .= "\nstruct" }; timeit("replace 0 with struct if it is located before a field instruction");

my $nextToken = "";
my $replacement = "";
# /m makes ^ = start of line; /g is required so that the next search will start at pos instead of 0
while (/(?<=\n)(\ck(?<theToken>$nextToken[^\ck\n]+)\ck(?<tabs>[\t]*))/mg) {
	if ($+{theToken} eq "new") {
		$replacement = "$+{tabs}\n$+{tabs}headerless\n$+{tabs}";
		substr($_, $-[1], $+[1] - $-[1], $replacement);
		pos = $-[1] + length($replacement);
		$nextToken = "[ne][ax]";
	}
	elsif ($+{theToken} eq "named") {
		$replacement = "$+{tabs}\n$+{tabs}headers\n$+{tabs}";
		substr($_, $-[1], $+[1] - $-[1], $replacement);
		pos = $-[1] + length($replacement);
		$nextToken = "[ne][ex]";
	}
	elsif ($+{theToken} eq "external") {
		$replacement = "$+{tabs}\n$+{tabs}external\n$+{tabs}";
		substr($_, $-[1], $+[1] - $-[1], $replacement);
		pos = $-[1] + length($replacement);
		$nextToken = "n";
	}
} timeit("add headerless, headers and external compiler directives");

s/(?<=\n)\ck[^\ck\n]+\ck//mg || do { $missing .= "\nnew, named, external" }; timeit("new, named, external");

s/\ck//g;


#s/([ \t\n])\(unnamed-fcode\)( \\ \[0x(${fcodep})\] )/$1unnamed-fcode_$3$2/g || do { $missing .= "\n(unnamed-fcode)" }; timeit("unnamed-fcode");

s/(?<=\n)(${indents})b\(\;\) ((?:\\ \[0x0c2\] )?)/$1; $2\n$1/mg || do { $missing .= "\nb(;)" }; timeit("b(;) -> ;");
s/(?<=\n)(${indents})b\(\'\) ((?:\\ \[0x011\] )?)([^ ]+ )((?:\\ \[0x${fcodep}\] )?)\n/$1\[\'\] $3$2$4\n/mg || do { $missing .= "\nb(')" }; timeit("b(') -> [']");
s/(?<=\n)(${indents})b\(\"\) ((?:\\ \[0x012\] )?)(.*)/$1$3$2/mg || do { $missing .= "\nb(\")" }; timeit('b(") -> "');
s/(?<=\n)(${indents})code,s((?: \\ \[0x0f8\])?) (.*)/$1$3$2/mg || do { $missing .= "\ncode,s" }; timeit("code,s -> code<<< >>> # Power Mac Quad G5");

if (0) {
	# lets not rename the new names to the old names
	s/(?<=\n)(${indents})lshift /$1<< /mg || do { $missing .= "\n<<" }; timeit("lshift -> <<");
	s/(?<=\n)(${indents})rshift /$1>> /mg || do { $missing .= "\n>>" }; timeit("rshift -> >>");
	s/(?<=\n)(${indents})chars /$1\/c* /mg || do { $missing .= "\n/c*" }; timeit("chars -> /c*");
	s/(?<=\n)(${indents})char\+ /$1ca1+ /mg || do { $missing .= "\nca1+" }; timeit("char+ -> ca1+");
	s/(?<=\n)(${indents})cells /$1\/n* /mg || do { $missing .= "\n/n*" }; timeit("cells -> /n*");
	s/(?<=\n)(${indents})cell\+ /$1na1+ /mg || do { $missing .= "\nna1+" }; timeit("cell+ -> na1+");
	s/(?<=\n)(${indents})invert /$1not /mg || do { $missing .= "\nnot" }; timeit("invert -> not");
	s/(?<=\n)(${indents})evaluate /$1eval /mg || do { $missing .= "\neval" }; timeit("evaluate -> eval");
}

s/(?<=\n)(${indents})\@ ((?:\\ \[0x06d\] )?)\n${indents}\. ((?:\\ \[0x09d\] )?)/$1? $2$3/mg || do { $missing .= "\n?" }; timeit("@ . -> ?");
s/(?<=\n)(${indents}[1-2]) ((?:\\ \[0x${fcodep}\] )?)\n${indents}([-+]) ((?:\\ \[0x${fcodep}\] )?)/$1$3 $2$4/mg || do { $missing .= "\n1+ 1- 2+ 2-" }; timeit("1 + 1 - 2 + 2 - -> 1+ 1- 2+ 2-");
s/(?<=\n)(${indents})(\" .*?)\n${indents}type /$1.$2/mg || do { $missing .= "\n.\"" }; timeit('" type -> ."');
s/(?<=\n)(${indents})bl ((?:\\ \[0x0a9\] )?)\n${indents}fill ((?:\\ \[0x079\] )?)/$1blank $2$3/mg || do { $missing .= "\nblank" }; timeit("bl fill -> blank");

s/(?<=\n)(${indents})(?:0a|d#10) ((?:\\ \[0x${fcodep}\] )?)\n${indents}base ((?:\\ \[0x0a0\] )?)\n${indents}! /$1decimal $2$3/mg || do { $missing .= "\ndecimal" }; timeit("0a base ! -> decimal");
s/(?<=\n)(${indents})8 ((?:\\ \[0x${fcodep}\] )?)\n${indents}base ((?:\\ \[0x0a0\] )?)\n${indents}! /$1octal $2$3/mg || do { $missing .= "\noctal" }; timeit("8 base ! -> octal");
s/(?<=\n)(${indents})(?:10|d#16) ((?:\\ \[0x${fcodep}\] )?)\n${indents}base ((?:\\ \[0x0a0\] )?)\n${indents}! /$1hex $2$3/mg || do { $missing .= "\nhex" }; timeit("10 base ! -> hex");

s/(?<=\n)(${indents})0 \\ \[0x0a5\] \n${indents}max \\ \[0x02f\] \n${indents}0 \\ \[0x0a5\] \n${indents}b\(\?do\) \\ \[0x018\] 0x7\n${indents}0 \\ \[0x0a5\] \n${indents}c, \\ \[0x0d0\] \n${indents}b\(loop\) \\ \[0x015\] 0x(?:ffffffff)?(?:ffff)?fffd\b/$1allot \\ [0x0a5] \\ [0x02f] \\ [0x0a5] \\ [0x018] \\ [0x0a5] \\ [0x0d0] \\ [0x015] /mg || do {
	s/(?<=\n)(${indents})0 \n${indents}max \n${indents}0 \n${indents}b\(\?do\) 0x7\n${indents}0 \n${indents}c, \n${indents}b\(loop\) 0x(?:ffffffff)?(?:ffff)?fffd\b/$1allot /mg || do { $missing .= "\nallot" };
}; timeit("0 max 0 b(?do) 0 c, b(loop) -> allot");

s/(?<=\n)(${indents})span \\ \[0x088\] \n${indents}@ \\ \[0x06d\] \n${indents}-rot \\ \[0x04b\] \n${indents}expect \\ \[0x08a\] \n${indents}span \\ \[0x088\] \n${indents}@ \\ \[0x06d\] \n${indents}swap \\ \[0x049\] \n${indents}span \\ \[0x088\] \n${indents}! /$1accept \\ [0x088] \\ [0x06d] \\ [0x04b] \\ [0x08a] \\ [0x088] \\ [0x06d] \\ [0x049] \\ [0x088] /mg || do {
	s/(?<=\n)(${indents})span \n${indents}@ \n${indents}-rot \n${indents}expect \n${indents}span \n${indents}@ \n${indents}swap \n${indents}span \n${indents}! /$1accept /mg || do { $missing .= "\naccept" };
}; timeit("span @ -rot expect span @ swap span ! -> accept");

s/(?<=\n)(${indents})>r \\ \[0x030\] \n${indents}over \\ \[0x048\] \n${indents}r@ \\ \[0x032\] \n${indents}\+ \\ \[0x01e\] \n${indents}swap \\ \[0x049\] \n${indents}r@ \\ \[0x032\] \n${indents}- \\ \[0x01f\] \n${indents}rot \\ \[0x04A\] \n${indents}r> /$1decode-bytes \\ [0x030] \\ [0x048] \\ [0x032] \\ [0x01e] \\ [0x049] \\ [0x032] \\ [0x01f] \\ [0x04A] /mg || do {
	s/(?<=\n)(${indents})>r \n${indents}over \n${indents}r@ \n${indents}\+ \n${indents}swap \n${indents}r@ \n${indents}- \n${indents}rot \n${indents}r> /$1decode-bytes /mg || do { $missing .= "\ndecode-bytes" };
}; timeit(">r over r@ + swap r@ - rot r> -> decode-bytes");

s/(?<=\n)(${indents})base \\ \[0x0a0\] \n${indents}@ \\ \[0x06d\] \n${indents}swap \\ \[0x049\] \n${indents}decimal ((?:\\ \[0x${fcodep}\] )*)\n${indents}\. \\ \[0x09d\] \n${indents}base \\ \[0x0a0\] \n${indents}! \\ \[0x072\] /$1.d \\ [0x0a0] \\ [0x06d] \\ [0x049] $2\\ [0x09d] \\ [0x0a0] \\ [0x072] /mg || do {
	s/(?<=\n)(${indents})base \n${indents}@ \n${indents}swap \n${indents}decimal \n${indents}\. \n${indents}base \n${indents}! /$1.d/mg || do { $missing .= "\n.d" };
}; timeit("base @ swap decimal . base ! -> .d");

s/(?<=\n)(${indents})base \\ \[0x0a0\] \n${indents}@ \\ \[0x06d\] \n${indents}swap \\ \[0x049\] \n${indents}hex ((?:\\ \[0x${fcodep}\] )*)\n${indents}\. \\ \[0x09d\] \n${indents}base \\ \[0x0a0\] \n${indents}! \\ \[0x072\] /$1.h \\ [0x0a0] \\ [0x06d] \\ [0x049] $2\\ [0x09d] \\ [0x0a0] \\ [0x072] /mg || do {
	s/(?<=\n)(${indents})base \n${indents}@ \n${indents}swap \n${indents}hex \n${indents}\. \n${indents}base \n${indents}! /$1.h/mg || do { $missing .= "\n.h" };
}; timeit("base @ swap hex . base ! -> .h");

s/(?<=\n)(${indents})dup ((?:\\ \[0x047\] )?)\n${indents}abs ((?:\\ \[0x02d\] )?)\n${indents}<# ((?:\\ \[0x096\] )?)\n${indents}u#s ((?:\\ \[0x09a\] )?)\n${indents}swap ((?:\\ \[0x049\] )?)\n${indents}sign ((?:\\ \[0x098\] )?)\n${indents}u#> /$1(.) $2$3$4$5$6$7/mg || do { $missing .= "\n(.)" }; timeit("dup abs <# u#s swap sign u#> -> (.)");

s/(?<=\n)(${indents})2 ((?:\\ \[0x${fcodep}\] )?)\n${indents}pick ((?:\\ \[0x04e\] )?)\n${indents}2 ((?:\\ \[0x${fcodep}\] )?)\n${indents}pick ((?:\\ \[0x04e\] )?)\n${indents}2 ((?:\\ \[0x${fcodep}\] )?)\n${indents}pick /${1}3dup $2$3$4$5$6/mg || do { $missing .= "\n3dup" }; timeit("2 pick 2 pick 2 pick -> 3dup");
s/(?<=\n)(${indents})l?b\?branch ((?:\\ \[(?:0x014|0x3fe)\] )?)0x4\n${indents}b\(leave\) ((?:\\ \[0x01b\] )?)\n${indents}b\(\>resolve\) /$1\?leave $2$3/mg || do { $missing .= "\n?leave" }; timeit("b?branch b(leave) b(>resolve) -> ?leave");
s/(?<=\n)(${indents})bl ((?:\\ \[0x0a9\] )?)\n${indents}emit /$1space $2/mg || do { $missing .= "\nspace" }; timeit("bl emit -> space");
s/(?<=\n)(${indents})\(\.\) ((?:\\ \[0x${fcodep}\] )*)\n${indents}type ((?:\\ \[0x090\] )?)\n${indents}space /$1s. $2$3/mg || do { $missing .= "\ns." }; timeit("(.) type space -> s.");

s/(?<=\n)(${indents})0 \\ \[0x0a5\] \n${indents}max \\ \[0x02f\] \n${indents}0 \\ \[0x0a5\] \n${indents}b\(\?do\) \\ \[0x018\] 0x7\n${indents}space \\ \[0x0a9\] \\ \[0x08f\] \n${indents}b\(loop\) \\ \[0x015\] 0x(?:ffffffff)?(?:ffff)?fffd\b/$1spaces \\ [0x0a5] \\ [0x02f] \\ [0x0a5] \\ [0x018] \\ [0x0a9] \\ [0x08f] \\ [0x015] /mg || do {
	s/(?<=\n)(${indents})0 \n${indents}max \n${indents}0 \n${indents}b\(\?do\) 0x7\n${indents}space \n${indents}b\(loop\) 0x(?:ffffffff)?(?:ffff)?fffd\b/$1spaces /mg || do { $missing .= "\nspaces" };
}; timeit("0 max 0 b(?do) space b(loop) -> spaces");

s/(?<=\n)(${indents})<# ((?:\\ \[0x096\] )?)\n${indents}u#s ((?:\\ \[0x09a\] )?)\n${indents}u#> /$1(u.) $2$3/mg || do { $missing .= "\n(u.)" }; timeit("<# u#s u#> -> (u.)");
s/(?<=\n)(${indents})drop ((?:\\ \[0x046\] )?)\n${indents}2drop /${1}3drop $2/mg || do { $missing .= "\n3drop" }; timeit("drop 2drop -> 3drop");
s/(?<=\n)(${indents})0 ((?:\\ \[0x${fcodep}\] )?)\n${indents}fill /$1erase $2/mg || do { $missing .= "\nerase" }; timeit("0 fill -> erase");

s/(?<=\n)(${indents})b\(([a-z+?]+)\) ((?:\\ \[0x${fcodep}\] )?)0x(?:ffffffff)?(?:ffff)?([89a-f][\da-f]{3})\b/$1$2 \\ \(0x$4\) $3/mg || do { $missing .= "\nb(loop,do,?do,endof,of) negative" }; timeit("b(loop) b(+loop) b(do) b(?do) b(endof) b(of) negative offset");
s/(?<=\n)(${indents})b\(([a-z+?]+)\) ((?:\\ \[0x${fcodep}\] )?)(0x[\da-f]+)/$1$2 \\ \($4\) $3/mg || do { $missing .= "\nb(loop,do,?do,endof,of)" }; timeit("b(loop) b(+loop) b(do) b(?do) b(endof) b(of)");

s/(?<=\n)(${indents})b\(to\) ((?:\\ \[0x0c3\] )?)([^ ]+ )((?:\\ \[0x${fcodep}\] )?)\n/${1}to $3$2$4\n/mg || do { $missing .= "\nb(to)" }; timeit("b(to) -> to");

s/(?<=\n)(${indents})b\(([a-z+?]+)\) /$1$2 /mg || do { $missing .= "\nb(leave,case,endcase)" }; timeit("b(leave) b(case) b(endcase) -> leave case endcase");

s/(?<=\n)(${indents})b\(<mark\) /$1begin /mg || do { $missing .= "\nbegin" }; timeit("begin");
s/(?<=\n)(${indents})l?b\?branch ((?:\\ \[(?:0x014|0x3fe)\] )?)0x(?:ffffffff)?(?:ffff)?([89a-f][\da-f]{3})\b/$1until \\ (0x$3) $2/mg || do { $missing .= "\nuntil" }; timeit("until");
s/(?<=\n)(${indentsnotgreedy})${indent}l?bbranch ((?:\\ \[(?:0x013|0x3ff)\] )?)(0x[\da-f]{1,4})\n${indents}b\(>resolve\) /$1else \\ ($3) $2/mg || do { $missing .= "\nelse" }; timeit("else");
                  s/(?<=\n)(${indents})l?bbranch ((?:\\ \[(?:0x013|0x3ff)\] )?)(0x[\da-f]{1,4})\n${indents}b\(>resolve\) /$1else \\ ($3) $2/mg && do { print STDERR "Got bad Else.\n" }; timeit("else");
s/(?<=\n)(${indents})l?bbranch ((?:\\ \[(?:0x013|0x3ff)\] )?)0x(?:ffffffff)?(?:ffff)?([89a-f][\da-f]{3})\b/$1again \\ (0x$3) $2/mg || do { $missing .= "\nagain" }; timeit("again");
s/(?<=\n)(${indents})b\(>resolve\) /$1then /mg || do { $missing .= "\nthen" }; timeit("then");
s/(?<=\n)(${indents})l?b\?branch ((?:\\ \[(?:0x014|0x3fe)\] )?)(0x[\da-f]{1,4})/$1if \\ ($3) $2/mg || do { $missing .= "\nif" }; timeit("if");

my $repeats = 0;
#         1           2                                                         3                                      4     5                                             
while (/(?<=\n)(${indents})(if) \\ \(0x[\da-f]{1,4}\) (?:\\ \[(?:0x014|0x3fe)\] )?(\n)(?:\1${indent}[^\n]*\n)*\1${indent}(again( \\ \(0x[89a-f][\da-f]{3}\) (?:\\ \[(?:0x013|0x3ff)\] )?)\n\1then )/mg) {
	substr($_, $-[4], $+[4] - $-[4], "repeat$5");
	substr($_, $-[2], $+[2] - $-[2], "while");
	pos = $-[3] + 3; # while is 3 characters longer than if
	$repeats++;
} timeit("if ... again then (from previous transformations above) -> while ... repeat");
if ($repeats == 0) { $missing .= "\nwhile ... repeat" }

s/(?<=\n)(${indents}): \clcolon_definition_function_\cl(${fcodep})\cl( .*\n\1${indent}\" ([^ \n\"]*)\" .*\n\1${indent}; )/$1: \clcolon_string_\cl$2_$4\cl$3/mg || do { $missing .= "\ncolon_string" }; timeit("colon_definition -> colon_string");
s/(?<=\n)(${indents}): \clcolon_definition_function_\cl(${fcodep})\cl( .*\n\1${indent}(${number}) .*\n\1${indent}; )/$1: \clcolon_const_\cl$2_$4\cl$3/mg || do { $missing .= "\ncolon_const" }; timeit("colon_definition -> colon_const");

my @fcodes;
while (/\cl([^\cl]+)\cl((${fcodep})[^\cl]*)\cl([^\cl]*)\cl[^\n]*(\n)/g) {
	push @fcodes, [$+[4], $1.$2.$4, $3]
} timeit("push fcodes");
for my $fcode (reverse @fcodes) {
#	pos = @$fcode[0];
#	while ( /\G.*?\b(unnamed_fcode_@$fcode[2]) [^\n]*(\n)/mgs ) {
#		substr($_, $-[1], $+[1] - $-[1], @$fcode[1]);
#		pos = $+[2];
#	}
	substr($_, @$fcode[0]) =~ s/(?<=\s)unnamed_fcode_@$fcode[2](?=\s)/@$fcode[1]/g
} timeit("rename unnamed fcodes that we have given names to");
s/\cl//g; timeit("remove control-l");

# outdent (then, until, again, loop, +loop, endcase, and repeat) but don't outdent else because it was done above
s/(?<=\n)(${indentsnotgreedy})${indent}(then|until|again|endcase|endof|[+]?loop)\b/$1$2/mg || do { $missing .= "\nindent1" }; timeit("indent1");
s/(?<=\n)(${indentsnotgreedy})${indent}${indent}(repeat)\b/$1$2/mg || do { $missing .= "\nindent2" }; timeit("indent2");

#===================================================
# Do formatting


my $t = "";
s/(?<=\n)(?{$t=""})(?:\t(?{$t.="    "}))+/$t/g; timeit("convert indent tabs to spaces");

#	while (/(?<=\n)(\t+)/mg) {
#		my $start = $-[1];
#		my $length = $+[1] - $-[1];
#		my $replacement = $1;
#		$replacement =~ s/\t/    /g;
#		substr($_, $start, $length, $replacement);
#		pos = $start + length($replacement);
#	}


# \ \ # the first slash is the name of the slash definition; the second slash is for the comment
s/ \\ \\/ \ck \\/mg || do { $missing .= "\nslash definition" }; timeit("protect slash definition");

my $_l = "";
my $_c = "";
my $_i = "";
my $_c2 = "";
my $_l2 = "";
my $_s = '                                                                                '; # lots of spaces
s/(?<=\n)(.+?) (\\[^\n["]*)([][()\\ \da-fx-]*)(?{
	# divide the line into the line part, comment part and instruction part.
	$_l = $1; # line
	$_c = $2; # comment
	$_i = $3; # instruction

	# clean up line (remove trailing spaces)
	$_l =~ s: +$::;

	# clean up comment (remove trailing backslash and spaces)
	$_c =~ s: \\ $::;
	$_c =~ s: +$::;

	#clean up the instructions (remove 0x and inner brackets)
	$_i =~ s:] \\ \[0x: :g;
	$_i =~ s:\[0x:[:;

	# calculate tabs between comment and instructions
	$_c2 = $_c.$_s;
	$_c2 =~ s:^(.{19} ) *$:$1:;			# comment that fits
	$_c2 =~ s:^(.{19}.*[^ ] ) *$:$1:;	# comment that does not fit
	$_c2 =~ s:^.*[^ ]( +)$:$1:;			# just grab the spaces
	$_c2 =~ s:    :\t:g;				# convert to tabs
	$_c2 =~ s: +:\t:;					# convert remaining spaces to a tab

	# calculate tabs between the line and comment
	$_l2 = $_l.$_s;
	$_l2 =~ s:^(.{79} ) *$:$1:;			# line that fits
	$_l2 =~ s:^(.{79}.*[^ ] ) *$:$1:;	# line that does not fits
	$_l2 =~ s:^.*[^ ]( +)$:$1:;			# just grab the spaces
	$_l2 =~ s:    :\t:g;				# convert to tabs
	$_l2 =~ s: +:\t:;					# convert remaining spaces to a tab
})$/$_l$_l2$_c$_c2$_i/mg; timeit("convert tabs to spaces");

s/\ck/\\/g; timeit("restore slash definition");

s/(?<=\n)(?{$t=""})(?:    (?{$t.="\t"}))+/$t/g; timeit("convert indent spaces to tabs");
#	while (/(?<=\n)((?:    )+)/mg) {
#		my $start = $-[1];
#		my $length = $+[1] - $-[1];
#		my $replacement = $1;
#		$replacement =~ s/    /\t/g;
#		substr($_, $start, $length, $replacement);
#		pos = $start + length($replacement);
#	}

s/[ \t]+$//mg; timeit("remove trailing spaces and tabs");


#===================================================

while (/(?<=\n)(.*?unnamed_fcode_.*\n)/mg) {
	print STDERR $1;
} #timeit("dump debug stuff");

print STDERR "\nThe following were not encountered:\n".$missing."\n";

print STDOUT $_;

#timeit("done");
 
exit 0;


=begin GHOSTCODE

" quote
stack: ( [text<">< >] -- text-str text-len )
code: none
generates: b(") len-byte xx-byte ... xx-byte
Gathers the immediately following text string or hex data until reaching the terminator
"<whitespace>.


." dot quote
stack: ( [text<">] -- )
code: none
generates: b(") len text type
This word compiles a text string, delimited by "<whitespace> e.g. ." hello
world" .
At execution time, the string is displayed. This word is equivalent to using " text"
type .
." is normally used only within a definition. The text string will be displayed later
when that definition is called. You may wish to follow it with cr to push out the text
buffer immediately. Use .( for any printing to be done immediately.


s"
stack: ( [text<">] -- text-str text-len )
generates: b(") len-byte xx-byte xx-byte ... xx-byte
Gather the immediately-following string delimited by " . Return the location of the
string text-str text-len.
Since an implementation is only required to provide two temporary buffers, a program
cannot depend on the system's ability to simultaneously maintain more than two
distinct interpreted strings. Compiled strings do not have this limitation, since they are
not stored in the temporary buffers.


==============
bbranch 		(Tokenized by again, repeat, and else)
 -offset for again and repeat
 for repeat, b(>resolve) follows bbranch

+ +offset for else
+ b(>resolve) follows bbranch
+ followed by another b(>resolve) later for then

==============
 b?branch       (Tokenized by until, while, and if)
+ -offset for until
 +offset for if and while

==============
 b(>resolve)    (Tokenized by else, then, and repeat)
 Target of forward bbranch or b?branch.

==============
+ b(<mark)       (Tokenized by begin)
+ target of backward bbranch or b?branch
+
==============
==============
 then
 Terminate an if statement.
==============
+
+ begin
+ Begin a conditional loop.
+ Tokenizer equivalent: b(<mark)
==============
 if
 Tokenizer equivalent: b?branch +offset

 while
 Mark first clause of a begin...while...repeat loop.
 Tokenizer equivalent: b?branch +offset
==============
+ else
+ Tokenizer equivalent: bbranch +offset b(>resolve)
==============
 repeat
 Mark end of a begin...while...repeat loop. Jump to begin.
 Tokenizer equivalent: bbranch -offset b(>resolve)

 again
 End an (infinite) begin...again loop.
 Tokenizer equivalent: bbranch -offset
==============
+ until
+ End a begin...until loop. Exit loop if flag is nonzero.
+ Tokenizer equivalent: b?branch -offset
==============


b(<mark) ... bbranch -offset                                         begin ... again
b(<mark) ... b?branch -offset                                        begin ... until
bbranch +offset ... b(>resolve)                                      ... else ... then
b?branch +offset ... b(>resolve)                                     if ... then
b(<mark) ... b?branch +offset ... bbranch -offset ... b(>resolve)    begin ... while ... repeat


bbranch     ( -- )             Unconditional branch FCode. Followed by offset.
b?branch    ( continue? -- )   Conditional branch FCode. Followed by offset.
b(<mark)    ( -- )             Target of backward branches.
b(>resolve) ( -- )             Target of forward branches.

==========================================================================================
To Do:
Number the structs and include field offset in the name before field size.


==========================================================================================

=end GHOSTCODE
=cut
