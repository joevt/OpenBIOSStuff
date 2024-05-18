#!/bin/bash

if (( 0 )); then
        exit 0

#	# To dump the Open Firmware dictionary:
#	#
#	# Run Terminal.app and set the terminal width to 130 and make sure the unlimited buffer option is selected in the window preferences.
#	# On the other computer, enter Open Firmware and type: " enet:telnet,192.168.1.150" io
#	# Then in the terminal type the following:
#	
#	telnet
#	open 192.168.1.150
#	
#	: dumpbytes
#	cr bounds ?do i 3f and 0= if cr then i c@ 2 u.r loop cr ;
#	
#	
#	# The first word in the dictionary is the last one listed when using the words command.
#	# Use ' to get the address of the word.
#	
#	0 > ' <init-world> . ff845730  ok			
#	
#	# Use dump to find memory before the first word that may contain stuff that you want to include in the disassembly
#	
#	# Then dump everything from that address to the end of the dictionary (which is given by the word "here")
#	0 > ff844b00 here ff844b00 - dumpbytes
#	
#	# If your Mac has a serial port then that can be used instead of telnet. Use the patch described in "Serial Port OF Mods"
#	# to set the serial port's speed to 230400 bps before starting.
fi


dumpRomDictionary=0
romStartOffset=0

while ((1)); do
	if [[ "${1}" == "-d" ]]; then
		dumpRomDictionary=1
		shift
	elif [[ "${1}" == "-s" ]]; then
		shift
		romStartOffset="${1}"
		shift
	elif [[ "${1}" == "-m" ]]; then
		shift
		macrom="${1}"
		shift
	else
		break
	fi
done

if (( $# < 1 )); then
	echo "### Error in parameter list"
	echo "### Usage - DumpMacRom.sh [-d] [-s startoffset] romdumpfile"
	echo "#"
	echo '# Dumps Open Firmware part of Old World ROM (0x30000 bytes at 0xFFF30000) including assembly part'
	echo "# Also works with memory dumps of the Open Firmware dictionary."
	echo "# 		-d			dump rom dictionary"
	echo "# 		-s			address of rom dump start"
	echo "# 		-m			which mac rom (overrides detection)"
	exit 1
fi

romFile="$1"

curDir="$(pwd)"
romDir="$(stat -f "%R" "$(dirname "$romFile")")"
#TempFolder=/tmp/DumpMacRom/ ; mkdir -p "${TempFolder}"
TempFolder="$(mktemp -d /tmp/DumpMacRom.XXXXXX)"

#echo "# Current directory: $curDir"
#echo "# Temp folder: $TempFolder"
#echo "# Rom folder: $romDir"


DumpMacRomDoErrors () {
	theError="${1}"
	tmpErrors="${TempFolder}/tmpErrors.txt"
	
	sed -E "/${theError}/ { w ${tmpErrors}
		d ; }" remainingErrors.txt > remainingErrors2.txt
	echo $(wc -l "${tmpErrors}") "${theError}" >> errorCounts.txt
	cat "${tmpErrors}" >> errors2.txt
	rm remainingErrors.txt
	mv remainingErrors2.txt remainingErrors.txt
}

{
	pushd "${curDir}" > /dev/null


	# Init variables

	dstFile="All.txt"
	dstFile2="Part1.txt"
	dstFile3="Part2.txt"
	dstFile4="Part2.of"


	# detok

	if [[ -z $macrom ]]; then

		{ grep -q 'Open Firmware, 0.992j' "${romFile}" && macrom=$((0x100)) ; } || \
		{ grep -q 'Open Firmware, 1.0.5'  "${romFile}" && macrom=$((0x01)) ; } || \
		{ grep -q 'Open Firmware, PipPCI' "${romFile}" && macrom=$((0x01)) ; } || \
		{ grep -q 'Open Firmware, 2.0f1'  "${romFile}" && macrom=$((0x02)) ; } || \
		{ grep -q 'Open Firmware 2.0a9'   "${romFile}" && macrom=$((0x02)) ; } || \
		{ grep -q 'Open Firmware, 2.0.1'  "${romFile}" && macrom=$((0x02)) ; } || \
		{ grep -q 'Open Firmware, 2.0.2'  "${romFile}" && macrom=$((0x200)) ; } || \
		{ grep -q 'Open Firmware, 2.0.3'  "${romFile}" && macrom=$((0x200)) ; } || \
		{ grep -q 'Open Firmware, 2.0'    "${romFile}" && macrom=$((0x12)) ; } || \
		{ grep -q 'Open Firmware 2.3'     "${romFile}" && macrom=$((0x02)) ; } || \
		{ grep -q 'Open Firmware, 2.4'    "${romFile}" && macrom=$((0x02)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '4.2.8f1' "${romFile}" && macrom=$((0x20)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '4.4.8f2' "${romFile}" && macrom=$((0x04)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '4.6.0f1' "${romFile}" && macrom=$((0x04)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '4.8.7f1' "${romFile}" && macrom=$((0x04)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '4.8.9f4' "${romFile}" && macrom=$((0x04)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '4.9.5f3' "${romFile}" && macrom=$((0x04)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && macrom=$((0x08)) ; } || \
		{ grep -q 'OpenFirmware 4'        "${romFile}" && macrom=$((0x04)) ; } || \
		{
			echo "# unknown Open Firmware version"
			macrom=$((0x04))
		}

	fi
	
	printf "# macrom = 0x%x\n" "${macrom}"

	#echo "# " detok -t -v -a -n -o -i -m '"'"${macrom}"'"' -s '"'"${romStartOffset}"'"' '"'"${romFile}"'"' " 2> " '"'"${TempFolder}/errors.txt"'"' " > " '"'"${TempFolder}/All0.txt"'"'
	detok -t -v -a -n -o -i -m "${macrom}" -s "${romStartOffset}" "${romFile}" 2> "${TempFolder}/errors.txt" > "${TempFolder}/All0.txt"

	# Add file command to errors
	perl -pE "s|^Line (\d+)(.*)|bbedit '${dstFile}':\1\2|" < "${TempFolder}/errors.txt" > "errors.txt"


	# Remove all the zeros at the end of the rom

	if [[ !${dumpRomDictionary} ]]; then
		perl -0777 -pE '
			s/
				(
					\\\ detokenizing\ finished.*\n
					(?:.*\n)*?
				)
				(?:[0-9A-F :]+:\ (?:0000\ )+\ +"\.+"\n)+
				([0-9A-F :]+:\ \\\ detokenizing\ finished.*\n)
				\n*
				$
			/\1\2/x
		' < "${TempFolder}/All0.txt" > "${dstFile}"
	else
		cat "${TempFolder}/All0.txt" > "${dstFile}"
	fi


	# Split the dump at the first start fcode

	perl -nE '
		if ( /^[0-9A-F :]+:\ \\\ detokenizing\ start/ .. /^[0-9A-F :]+:\ \\\ detokenizing\ finished/ ) {
			if ( /^[0-9A-F :]+:\ \\\ detokenizing/ ) { print $_; }
		} else { print $_; }
	' < "${dstFile}" > "${dstFile2}"

	perl -nE 'if ( /^[0-9A-F :]+:\ \\\ detokenizing\ start/ .. /^[0-9A-F :]+:\ \\\ detokenizing\ finished/ ) { print $_; }
	' < "${dstFile}" > "${dstFile3}"

	diff -q "${dstFile}" "${dstFile2}" > /dev/null && rm "${dstFile2}"


	# Find bad errors by removing the benign ones

	cat errors.txt > remainingErrors.txt
	echo -n > remainingErrors2.txt
	echo -n > errorCounts.txt
	echo -n > errors2.txt

		DumpMacRomDoErrors '# Defined extended range token'
		DumpMacRomDoErrors '# Defined named visible token'
		DumpMacRomDoErrors '# Defined named invisible token'
		DumpMacRomDoErrors '# Renamed program defined token'
		DumpMacRomDoErrors '# Renamed extended range token'
		DumpMacRomDoErrors '# Undefined'
		DumpMacRomDoErrors '# Redefined program defined token'
		DumpMacRomDoErrors '# Defined program defined token'
		DumpMacRomDoErrors '# Defined headerless token'
		DumpMacRomDoErrors '# Renamed negative range token'
		DumpMacRomDoErrors '# Defined instance token'
		DumpMacRomDoErrors '# Defined predefined reserved token'
		DumpMacRomDoErrors '# Defined headerless invisible token'
		DumpMacRomDoErrors '# Defined immediate token'
		DumpMacRomDoErrors '# Defined vectored token'
		DumpMacRomDoErrors '# Defined negative range token'
		DumpMacRomDoErrors '# Defined reserved token'
		DumpMacRomDoErrors '# Defined alias token'
		DumpMacRomDoErrors '# Defined predefined new token'
		DumpMacRomDoErrors '# Renamed predefined reserved token'
		DumpMacRomDoErrors '# Redefined extended range token'
		DumpMacRomDoErrors '# Defined predefined historical token'
		DumpMacRomDoErrors '# Renamed predefined historical token'
		DumpMacRomDoErrors '; Previous token offset is 0'
		DumpMacRomDoErrors '; Token offset points to'

#		DumpMacRomDoErrors '# Redefined predefined token'
#		DumpMacRomDoErrors '# Redefined new token'
#		DumpMacRomDoErrors '# Renamed predefined token'
#		DumpMacRomDoErrors '# Redefined historical token'

		DumpMacRomDoErrors '# Assembly instruction has invalid field'
		DumpMacRomDoErrors '# Possibly invalid special purpose register'

		DumpMacRomDoErrors 'because it should have a good name'

		DumpMacRomDoErrors '# Start'
		DumpMacRomDoErrors '# End'
		DumpMacRomDoErrors '# Unknown start'

		DumpMacRomDoErrors '# Indentation error'

	if ((0)); then
		# tokens should not have bad names
		DumpMacRomDoErrors '# Bad token name'

		DumpMacRomDoErrors '# Bad b(code) token name'
		DumpMacRomDoErrors '# Bad b(:) token name'
		DumpMacRomDoErrors '# Bad b(value) token name'
		DumpMacRomDoErrors '# Bad b(variable) token name'
		DumpMacRomDoErrors '# Bad b(constant) token name'
		DumpMacRomDoErrors '# Bad b(create) token name'
		DumpMacRomDoErrors '# Bad b(defer) token name'
		DumpMacRomDoErrors '# Bad b(buffer:) token name'
		DumpMacRomDoErrors '# Bad b(field) token name'
	fi

	if ((0)); then
		# tokens should not get renamed (unless they are some kind of alias)
		DumpMacRomDoErrors '# Renamed new token'
		DumpMacRomDoErrors '# Renamed reserved token'
		DumpMacRomDoErrors '# Renamed vendor unique token'
		DumpMacRomDoErrors '# Renamed historical token'
	fi
		DumpMacRomDoErrors '# Ignored attempt to rename'

	if ((0)); then
		# these should not get redefined
		DumpMacRomDoErrors '# Redefined reserved token'
		DumpMacRomDoErrors '# Redefined vendor unique token'
	fi

	if ((0)); then
		# these should not be defined
		DumpMacRomDoErrors '# Defined historical token'
		DumpMacRomDoErrors '# Defined new token'
		DumpMacRomDoErrors '# Defined predefined token'
	fi
		DumpMacRomDoErrors '# Defined vendor unique token'

	if ((0)); then
		# these tokens should not get used
		DumpMacRomDoErrors '# Historical or non-implemented FCode'
	fi
		DumpMacRomDoErrors '# New FCode'

	if ((1)); then
		# status messages
		DumpMacRomDoErrors '# Min negative FCode number is'
		DumpMacRomDoErrors '# Max FCode number is'
		DumpMacRomDoErrors 'Doing Pass'
	fi

	numErrors=$(wc -l < remainingErrors.txt)
	#echo "numErrors: ${numErrors}"
	if (( ${numErrors} == 0 )); then
		rm remainingErrors.txt
	fi
	
	sort -Vr errorCounts.txt -o errorCountsSorted.txt
	# bbdiff errors.txt errors2.txt

	egrep -n "\bferror" "${dstFile}" | perl -nE 's|^(\d+):|bbedit "'"${dstFile}"'":\1 #|; s/(.*:)[ \t]*(ferror.*)/\1 \2/; if ( !( /b[(]/ || / bl / || / dc.l / ) ) {print $_; } ' >  ferrors.txt

	if (( $(stat -f %z ferrors.txt) == 0 )); then
		rm ferrors.txt
	else
		echo bbedit '"'$( find "$(pwd)" -name 'ferrors.txt' -maxdepth 1 )'"'
		sed -E "/.*\[0x([0-9a-f]{3,4})\] */s// \1/; /^ *(.{4})$/s//\1/" ferrors.txt | sort
	fi

	sed -nE "/.*(mac_rom_[^ ]+).*/s//\1/p" "${dstFile3}" | sort -u > mac_rom_b_usage.txt
	if (( $(stat -f %z mac_rom_b_usage.txt) == 0 )); then
		rm mac_rom_b_usage.txt
	fi

	perl -nE '
		if ( !/0x[0-9A-Fa-f]{3,4}/ || / New FCode/ || / FCode number is / ) { }
		else {
			if ( /^((.*?:)([0-9]{1,5})( \#.*))/ ) {
				$p1 = $2; $p2 = $4; $linenum = ("00000" . $3) =~ s/0+(.{6})$/\1/r;
				s/.*/$p1$linenum$p2/;
			}
			if    ( /(.*?0x([0-9A-Fa-f]{4})\b.*)/ ) { print "a=\"0x"  . $2 . "\"; " . $1 . "\n" ; }
			elsif ( /(.*?0x([0-9A-Fa-f]{3})\b.*)/ ) { print "a=\" 0x" . $2 . "\"; " . $1 . "\n" ; }
			else { print $_ . "\n" ; }
		}
	' \
	errors.txt | sort > fcode_defines.txt

	sed -E "/^ *[0-9]+: [0-9A-Fa-f]+: (.*)/s//\1/" "${dstFile3}" > "${TempFolder}/Part2.txt"
	if (( $(stat -f %z "${TempFolder}/Part2.txt") == 0 )); then
		rm "${TempFolder}/Part2.txt"
	else
		#echo ConvertFCodeTokensToForth.pl '"'"${TempFolder}/Part2.txt"'"' '""' ">" '"'"${dstFile4}"'"' "2>" '"'"errors3.txt"'"' "|| {}"
		ConvertFCodeTokensToForth.pl "${TempFolder}/Part2.txt" "" > "${dstFile4}" 2> "errors3.txt" || {}
		if (( $(stat -f %z "errors3.txt") == 0 )); then
			rm "errors3.txt"
		fi
	fi

	if (( $(stat -f %z "${dstFile3}") == 0 )); then
		rm "${dstFile3}"
	fi

	popd > /dev/null

} || exit 1

exit 0
