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
hasRomStart=0

while ((1)); do
	if [[ "${1}" == "-d" ]]; then
		dumpRomDictionary=1
		shift
	elif [[ "${1}" == "-s" ]]; then
		shift
		romStartOffset="${1}"
		hasRomStart=1
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

#	mac_rom > 0xfff			undefined
#
#	mac_rom == 0			assume first byte is fcode
#	mac_rom != 0			assume first byte is not fcode - include disassembly of parts that are not fcode
#	mac_rom & 4				G5 (PPC 970fx) registers in disassembly
#
#	mac_rom & 2				0x010 start of fcode image			decode_rom_token2
#	mac_rom & 0x200			0x000 start of fcode image			decode_rom_token203
#	mac_rom & 0x4FC			0x000 start of fcode image			decode_rom_token3
#
#	mac_rom & 4				0x0f8 code<<< >>> 					decode_s
#	mac_rom != 0			0x3fe lb?branch, 0x3ff lbbranch		decode_branch(long)
#	mac_rom & 0x901			0x401 b(pushlocals)					local_variables_declaration
#	(mac_rom & 0x901) == 0	0x407 b(pushlocals), 0x408..0x40F
#
#	mac_rom & 0x100			unnamed tokens 0x430 to 0x44F
#	(mac_rom & 0x801) == 1	unnamed tokens 0x431 to 0x454
#	mac_rom & 0x1000		unnamed tokens 0x431 to 0x452
#	mac_rom & 0x10			unnamed tokens 0x43C to 0x45F
#	mac_rom & 0x200			unnamed tokens 0x43D to 0x461
#	mac_rom & 0x400			(same as mac_rom & 0x200)
#	mac_rom & 2				unnamed tokens 0x43E to 0x462
#	mac_rom & 8				unnamed tokens 0x438 to 0x458 and 0x3FE, 0x3FF
#	mac_rom & 0x24			tokens are named
#
#	(mac_rom & 0x24) == 0	fcode number cannot be > 0xFFF
#	(mac_rom & 0x20) != 0	fcode number cannot be > 0xFFF and < 0xFFFF
#	any values				fcode number cannot be > 0 and < 0x10
#	mac_rom & 4				0x0f4 16 bit fcode number

	# detok

	if [[ -z $macrom ]]; then

		{ grep -q 'Open Firmware, 0.992j' "${romFile}"                                    && macrom=$((0x100)) ; } || \
		{ grep -q 'Open Firmware, 1.0.5'  "${romFile}"                                    && macrom=$((0x001)) ; } || \
		{ grep -q 'OpenFirmware1.1.22'    "${romFile}"                                    && macrom=$((0x801)) ; } || \
		{ grep -q 'Open Firmware, PipPCI' "${romFile}"                                    && macrom=$((0x001)) ; } || \
		{ grep -q 'Open Firmware, 2.0f1'  "${romFile}"                                    && macrom=$((0x002)) ; } || \
		{ grep -q 'Open Firmware 2.0a9'   "${romFile}"                                    && macrom=$((0x002)) ; } || \
		{ grep -q 'Open Firmware, 2.0.1'  "${romFile}"                                                                \
		                        && grep -q -e '/cpus/PowerPC,603ev....cpu0' "${romFile}"  && macrom=$((0x402)) ; } || \
		{ grep -q 'Open Firmware, 2.0.1'  "${romFile}"                                    && macrom=$((0x002)) ; } || \
		{ grep -q 'Open Firmware, 2.0.2'  "${romFile}"                                    && macrom=$((0x200)) ; } || \
		{ grep -q 'Open Firmware, 2.0.3'  "${romFile}"                                    && macrom=$((0x200)) ; } || \
		{ grep -q 'Open Firmware, 2.0'    "${romFile}"                                    && macrom=$((0x012)) ; } || \
		{ grep -q 'Open Firmware 2.3'     "${romFile}"                                    && macrom=$((0x002)) ; } || \
		{ grep -q 'Open Firmware, 2.4'    "${romFile}"                                    && macrom=$((0x002)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '10/11/01' "${romFile}" && macrom=$((0x020)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '11/20/01' "${romFile}" && macrom=$((0x004)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '09/30/02' "${romFile}" && macrom=$((0x004)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '11/11/02' "${romFile}" && macrom=$((0x004)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '02/20/03' "${romFile}" && macrom=$((0x004)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '09/23/04' "${romFile}" && macrom=$((0x004)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '03/23/05' "${romFile}" && macrom=$((0x004)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}" && grep -q '09/22/05' "${romFile}" && macrom=$((0x004)) ; } || \
		{ grep -q 'OpenFirmware 3'        "${romFile}"                                    && macrom=$((0x008)) ; } || \
		{ grep -q 'OpenFirmware 4'        "${romFile}"                                    && macrom=$((0x004)) ; } || \
		{
			echo "# unknown Open Firmware version"
			macrom=$((0x04))
		}

	fi

	printf "# macrom = 0x%x\n" "${macrom}"

	if ((0)); then
		# Exclude the coff header if it exists.
		if ((hasRomStart == 0)); then
			coffheader="$(dd if="${romFile}" bs=0x3C count=1 2> /dev/null | xxd -p -c 100)"
			if perl -ne 'exit ((/^01df0001476172790{24}2e746578740{24}\w{6}0{6}3c0{30}20$/) ? 0 : 1)' <<< "$coffheader"; then
				romStartOffset="$((0x100000000-0x3c))"
			fi
		fi
	fi

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

	missingwords="$(
		perl -0777 -ne '
			$what = "(gdoes)"   ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 7E88 02A6 .*\n *\w+: *\w+: 7E69 03A6 .*\n *\w+: *\w+: 827E 0000 .*\n *\w+: *\w+: 3BDE 0004 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(val)"     ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E49 03A6 .*\n *\w+: *\w+: 7C68 02A6 .*\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 8283 0000 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(i-val)"   ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E49 03A6 .*\n *\w+: *\w+: 7C68 02A6 .*\n *\w+: *\w+: 809D 0000 .*\n *\w+: *\w+: 8063 0000 .*\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 7E84 182E .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "b<to>"     ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E48 02A6 .*\n *\w+: *\w+: 3872 0004 .*\n *\w+: *\w+: 8092 0000 .*\n *\w+: *\w+: 7C69 03A6 .*\n *\w+: *\w+: 9284 000[08] .*\n *\w+: *\w+: 829F 0000 .*\n *\w+: *\w+: 3BFF 0004 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "b<to>1"    ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E48 02A6 .*\n *\w+: *\w+: 8072 0000 .*\n *\w+: *\w+: 8092 FFFC .*\n *\w+: *\w+: 7C72 1A14 .*\n *\w+: *\w+: 9072 0000 .*\n *\w+: *\w+: 3804 FFD[08] .*\n *\w+: *\w+: 5004 01BA .*\n *\w+: *\w+: 9492 FFFC .*\n *\w+: *\w+: 7C00 906C .*\n( *\w+: *\w+: 7C00 04AC .*\n *\w+: *\w+: 4C00 012C .*\n)? *\w+: *\w+: 7C00 97AC .*\n *\w+: *\w+: 7C00 04AC .*\n *\w+: *\w+: 4C00 012C .*\n *\w+: *\w+: 4BFF FF\w\w .* b +(b<to>|mac_rom_code_\w+) .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(i-to)"    ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E48 02A6 .*\n *\w+: *\w+: 8072 0000 .*\n *\w+: *\w+: 809D 0000 .*\n *\w+: *\w+: 8063 000[08] .*\n *\w+: *\w+: 3812 0004 .*\n *\w+: *\w+: 7C09 03A6 .*\n *\w+: *\w+: 7E84 192E .*\n *\w+: *\w+: 829F 0000 .*\n *\w+: *\w+: 3BFF 0004 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(var)"     ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E49 03A6 .*\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 7E88 02A6 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(i-var)"   ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E49 03A6 .*\n *\w+: *\w+: 7C68 02A6 .*\n *\w+: *\w+: 809D 0000 .*\n *\w+: *\w+: 8063 0000 .*\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 7E84 1A14 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(defer)"   ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7C68 02A6 .*\n *\w+: *\w+: 8003 0000 .*\n *\w+: *\w+: 7E48 03A6 .*\n *\w+: *\w+: 4BFF FF\w\w .* b +execute\+12 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(i-defer)" ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7C68 02A6 .*\n *\w+: *\w+: 8063 0000 .*\n *\w+: *\w+: 809D 0000 .*\n *\w+: *\w+: 7E48 03A6 .*\n *\w+: *\w+: 7C04 182E .*\n *\w+: *\w+: 4BFF FF\w\w .* b +execute\+12 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "(field)"   ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7C68 02A6 .*\n *\w+: *\w+: 8003 0000 .*\n *\w+: *\w+: 7E49 03A6 .*\n *\w+: *\w+: 7E94 0214 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what1 = "b<lit>" ; $what2 = "b<'"'"'>" ; $name1 = ""; $name2 = ""; if (/ ([^\s]+)\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 7E48 02A6 .*\n *\w+: *\w+: 3872 0004 .*\n *\w+: *\w+: 7C69 03A6 .*\n *\w+: *\w+: 8292 0000 .*\n *\w+: *\w+: 4E80 0420 .*\n *\w+: *\w+: FFFF F\w\w\w .*\n *\w+: *\w+: \w\w .*\n.*? 0x\w+ (?:\\ )?([^\s]+)\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 7E48 02A6 .*\n *\w+: *\w+: 3872 0004 .*\n *\w+: *\w+: 7C69 03A6 .*\n *\w+: *\w+: 8292 0000 .*\n *\w+: *\w+: 4E80 0420 .*\n/)
				{ $name1 = $1; $name2 = $2; if ($name1 ne $what1) { printf("    $name1 = $what1\n"); } if ($name2 ne $what2) { printf("    $name2 = $what2\n"); } } else { printf("    $what1\n"); printf("    $what2\n"); }
			$what = "{'"'"'}"   ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 7E48 02A6 .*\n *\w+: *\w+: 8292 0000 .*\n *\w+: *\w+: 3812 0004 .*\n( *\w+: *\w+: 7C09 03A6 .*\n *\w+: *\w+: 5694 3032 .*\n *\w+: *\w+: 7E94 3670 .*\n| *\w+: *\w+: 5694 3032 .*\n *\w+: *\w+: 7C09 03A6 .*\n *\w+: *\w+: 7E94 3670 .*\n) *\w+: *\w+: 3800 0003 .*\n *\w+: *\w+: 7E94 0078 .*\n *\w+: *\w+: 7E94 9214 .*\n *\w+: *\w+: 4E80 0420 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
			$what = "b<\">"     ; $name = ""; if (/ ([^\s]+)\n *\w+: *\w+: 7E48 02A6 .*\n *\w+: *\w+: 969F FFFC .*\n *\w+: *\w+: 8A92 0000 .*\n *\w+: *\w+: 3892 0001 .*\n *\w+: *\w+: 7CA4 A214 .*\n *\w+: *\w+: 38A5 0003 .*\n *\w+: *\w+: 54A5 003A .*\n *\w+: *\w+: 7CA8 03A6 .*\n *\w+: *\w+: 949F FFFC .*\n *\w+: *\w+: 4E80 0020 .*\n/) { $name = $1; if ($name ne $what) { printf("    $name = $what\n"); } } else { printf("    $what\n"); }
		' "$dstFile2"
	)"

	if [[ -n $missingwords ]]; then
		printf "# Missing the following words:\n%s\n" "$missingwords"
	fi

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
		#echo ConvertFCodeTokensToForth.pl '"'"${TempFolder}/Part2.txt"'"' '""' ">" '"'"${dstFile4}"'"' "2>" '"'"errors3.txt"'"' "|| :"
		ConvertFCodeTokensToForth.pl "${TempFolder}/Part2.txt" "" > "${dstFile4}" 2> "errors3.txt" || :
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
