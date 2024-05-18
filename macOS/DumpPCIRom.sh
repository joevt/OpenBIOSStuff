#!/bin/bash

printf '========================\nDumpPCIRom.sh log:\n\n' 1>&2

script_name=$0
script_full_path=$(dirname "$0")

if [[ ! -f $script_full_path/ConvertFCodeTokensToForth.pl ]]; then
	script_full_path=$(dirname "$(command -v "$script_name")")
fi

echo "# Working Path: "$(pwd) 1>&2
echo "# Script Name: "$script_name 1>&2
echo "# Script Path: "$script_full_path 1>&2

extractpart=-1
isanother=0
listparts=0
dumppciheader=1
detokoptions=""
while ((1)); do
	if [[ $1 == "-v" ]]; then
		detokoptions="-v"
		shift
		continue
	fi
	if [[ $1 == "-e" ]]; then
		dumppciheader=0
		shift
		extractpart=$1
		shift
		continue
	fi
	if [[ $1 == "-ea" ]]; then
		dumppciheader=0
		isanother=1
		shift
		extractpart=$1
		shift
		continue
	fi
	if [[ $1 == "-l" ]]; then
		dumppciheader=0
		listparts=1
		shift
		continue
	fi
	break
done

if (( $# != 1 )) ; then
	echo "### Error in parameter list" 1>&2
	echo "### Usage - DumpPCIRom.sh [-v|-l|-e part|-ea part] romfile > output 2> messages" 1>&2
	echo "#" 1>&2
	echo "#         -v        output fcode numbers for all tokens" 1>&2
	echo "#         -l        list PCI option rom parts" 1>&2
	echo "#         -e part   extract part as an only image" 1>&2
	echo "#         -ea part  extract part as not an only image" 1>&2
	exit 1
fi

# 1='Work:Programming:CW Projects:DumpNameRegistry:ROM Radeon 7000:ROMs:Radeon 7000 v208'
# 1="${theDir}Radeon7000v208.fc"
theROM="$1"
RomStart=0
RomFileSize=$(wc -c < "${theROM}")

TempFolder="$(mktemp -d /tmp/DumpPCIRom.XXXXXX)"
echo "# TempFolder: "$TempFolder 1>&2

didDetok=0
detokErrors="/${TempFolder}/detokErrors.txt"
printf "" > "${detokErrors}"

part=0
numberOfPartsOutput=0

romhex="$(xxd -u -p -c 999999999 "${theROM}")"

while true; do
	printf "# Doing part: %d\n" "${part}" 1>&2

	#========================================
	# Dump PCI header
	
	vpd=$(xxd -p -r <<< "${romhex:(RomStart*2):28*2}" | xxd -u -g 4 -c 28)
	#echo "# got vpd: '$vpd'" 1>&2
	
	pat='^........: (....)(..)(..) (.{44}) (..)(..)(....)  ...(.)(.{20})....'
	if [[ ${vpd} =~ $pat ]] ; then
		PciMagicNumber=${BASH_REMATCH[1]}
		offset2=0
		codeTypeString="Open Firmware"
		IndicatorString="last image"
		ImageBytes="$RomFileSize"
	else
		echo "# bad vpd: '$vpd'" 1>&2
		exit 1
	fi
	
	pciHeaderFile=""
	if [[ ${PciMagicNumber} == 55AA ]] ; then
		pciHeaderFile="${TempFolder}/PCIHeader${part}"
		printf "# Got PCI Header\n" 1>&2
		BIOSImageLength=${BASH_REMATCH[2]}
		BIOSData="${BASH_REMATCH[3]} ${BASH_REMATCH[4]}  ${BASH_REMATCH[8]}${BASH_REMATCH[9]}"
		ProcessorArchitecture=${BASH_REMATCH[3]}${BASH_REMATCH[2]}
		ProcessorArchitectureUniqueData="${BASH_REMATCH[4]}  ${BASH_REMATCH[9]}"
		PointerPCIDataStructure=${BASH_REMATCH[6]}${BASH_REMATCH[5]}
		((PCIStart=RomStart + 0x${PointerPCIDataStructure}))
		PadBytes=${BASH_REMATCH[7]}
	
		pds=$(xxd -p -r <<< "${romhex:(PCIStart*2):24*2}" | xxd -u -g 4 -c 24)
		#echo "# got pds: '$pds'" 1>&2

		pat='^........: .{8} (..)(..)(..)(..) (..)(..)(..)(..) (..)(.{30}).{20}'
		if [[ ${pds} =~ $pat ]] ; then
			VendorID=${BASH_REMATCH[2]}${BASH_REMATCH[1]}
			DeviceID=${BASH_REMATCH[4]}${BASH_REMATCH[3]}
			PointerVitalProductData=${BASH_REMATCH[6]}${BASH_REMATCH[5]}
			PCIDataStructureLength=${BASH_REMATCH[8]}${BASH_REMATCH[7]}
			PCIDataStructureRevision=${BASH_REMATCH[9]}
			PCIInfo=${BASH_REMATCH[10]}
			#echo "# got PCIInfo: '$PCIInfo'" 1>&2
		else
			echo "# bad pds: '$pds'" 1>&2
			exit 1
		fi
	
		pat='(..)(..)(..) (..)(..)(..)(..) (..)(..)(....  ....)'
		if [[ $PCIInfo =~ $pat ]] ; then
			ClassCode=${BASH_REMATCH[3]}${BASH_REMATCH[2]}${BASH_REMATCH[1]}
			ImageLength=${BASH_REMATCH[5]}${BASH_REMATCH[4]}
			(( ImageBytes = 0x${ImageLength} * 512 ))
			ImageBytesHex=$(printf "%X" $ImageBytes)
			RevisionLevel=${BASH_REMATCH[7]}${BASH_REMATCH[6]}
			CodeType=${BASH_REMATCH[8]}
			Indicator=${BASH_REMATCH[9]}
			SignatureStuff=${BASH_REMATCH[10]}
			#echo "# got SignatureStuff: '$SignatureStuff'" 1>&2
		else
			echo "# bad ClassCode" 1>&2
			exit 1
		fi
	
		pat='(..)(..)  (....)'
		if [[ $SignatureStuff =~ $pat ]] ; then
			MaximumRunTimeImageLength=${BASH_REMATCH[2]}${BASH_REMATCH[1]} # Reserved in earlier versions
			SignatureString=${BASH_REMATCH[3]}
		else
			echo "# bad SignatureString" 1>&2
			exit 1
		fi
	
		(( offset2 = PCIStart + 0x${PCIDataStructureLength}))
	
		if [[ "${CodeType}" == "00" ]] ; then
			codeTypeString="BIOS"
		elif [[ "${CodeType}" == "01" ]] ; then
			codeTypeString="Open Firmware"
		elif [[ "${CodeType}" == "02" ]] ; then
			codeTypeString="Hewlett-Packard PA RISC"
		elif [[ "${CodeType}" == "03" ]] ; then
			codeTypeString="EFI"
			EFIInitializationSize=$ProcessorArchitecture
			EFISignature=${ProcessorArchitectureUniqueData:6:2}${ProcessorArchitectureUniqueData:4:2}${ProcessorArchitectureUniqueData:2:2}${ProcessorArchitectureUniqueData:0:2}
			EFISubSystem=${ProcessorArchitectureUniqueData:11:2}${ProcessorArchitectureUniqueData:9:2}
			EFIMachineType=${ProcessorArchitectureUniqueData:15:2}${ProcessorArchitectureUniqueData:13:2}
			EFICompressionType=${ProcessorArchitectureUniqueData:20:2}${ProcessorArchitectureUniqueData:18:2}
			EFIReserved=${ProcessorArchitectureUniqueData:22:2}${ProcessorArchitectureUniqueData:24:2}${ProcessorArchitectureUniqueData:27:2}${ProcessorArchitectureUniqueData:29:2}${ProcessorArchitectureUniqueData:31:2}${ProcessorArchitectureUniqueData:33:2}${ProcessorArchitectureUniqueData:36:2}${ProcessorArchitectureUniqueData:38:2}
			EFIOffsetToImage=${ProcessorArchitectureUniqueData:42:2}${ProcessorArchitectureUniqueData:40:2}

			if ((EFICompressionType == 0)); then
				EFICompressionTypeString=" = Uncompressed"
			elif ((EFICompressionType == 1)); then
				EFICompressionTypeString=" = Compressed"
			else
				EFICompressionTypeString=" = Reserved"
			fi
			
			if ((0x$EFISubSystem == 0xB)); then
				EFISubSystemString=" = EFI Boot Service Driver"
			elif ((0x$EFISubSystem == 0xC)); then
				EFISubSystemString=" = EFI Runtime Driver"
			else
				EFISubSystemString=" = ?"
			fi
			
			if ((0x$EFIMachineType == 0x014C)); then
				EFIMachineTypeString=" = IA-32"  
			elif ((0x$EFIMachineType == 0x0200)); then
				EFIMachineTypeString=" = Itanium processor type"
			elif ((0x$EFIMachineType == 0x0EBC)); then
				EFIMachineTypeString=" = EFI Byte Code (EBC)"
			elif ((0x$EFIMachineType == 0x8664)); then
				EFIMachineTypeString=" = X64"
			elif ((0x$EFIMachineType == 0x01c2)); then
				EFIMachineTypeString=" = ARM"
			elif ((0x$EFIMachineType == 0xAA64)); then
				EFIMachineTypeString=" = ARM 64-bit"
			else
				EFIMachineTypeString=" = ?"
			fi
		else
			codeTypeString="??"
		fi

		if [[ "${Indicator}" == "80" ]] ; then
			IndicatorString="last image"
		elif [[ "${Indicator}" == "00" ]] ; then
			IndicatorString="another image"
		else
			IndicatorString="??"
		fi
	
		if ((PCIDataStructureRevision == 3)); then
			PCIDataStructureRevisionString=" = PCI 3.0"
		elif ((PCIDataStructureRevision == 0)); then
			PCIDataStructureRevisionString=" = PCI 2.2"
		else
			PCIDataStructureRevisionString=""
		fi
	
		if [[ ${PointerPCIDataStructure} != 001C ]] ; then
			dump1=$((${RomStart} + 28))
			varBytes="$(
				xxd -p -r <<< "${romhex:(dump1)*2:$((0x${PointerPCIDataStructure} - 28))*2}" | xxd -u -o $dump1 -c 32 -g 4 | sed -E '/(.*)/s//\\\        \1/' | perl -pE 's/^(\\ +\w+:)/uc $1/e'
			)"
		else
			varBytes=""
		fi

		if (( 0x$PCIDataStructureLength != 0x0018 + (PCIDataStructureRevision == 3 ? 4 : 0) )) ; then
			dump1=$((PCIStart + 0x0018 + (PCIDataStructureRevision == 3 ? 4 : 0)))
			varBytes2="$(
				xxd -p -r <<< "${romhex:(dump1)*2:(offset2 - dump1)*2}" | xxd -u -o $dump1 -c 32 -g 4 | sed -E '/(.*)/s//\\\        \1/' | perl -pE 's/^(\\ +\w+:)/uc $1/e'
			)"
		else
			varBytes2=""
		fi
		#echo "# got varBytes2: '$varBytes2'" 1>&2
		
		if ((PCIDataStructureRevision >= 3)); then
			#                           ${VPDorDLP}
			VPDorDLP="Device list pointer"
			ReservedOrMRTIL="Maximum run-time image length"
			ReservedOrMRTIL2=" (* 0x200 = 0x$(printf "%X" $((MaximumRunTimeImageLength * 512))))"


			#                                     ${Entry_point}
			Entry_point="Entry point for INIT function (PCI 3.0)"

			pds2=$(xxd -p -r <<< "${romhex:(PCIStart + 24)*2:4*2}" | xxd -u -g 4 -c 4)
			pat='^........: (..)(..)(..)(..)  .*'
			if [[ ${pds2} =~ $pat ]] ; then
				PointertoConfigurationUtilityCodeHeader=${BASH_REMATCH[2]}${BASH_REMATCH[1]}
				PointertoDMTFCLPEntryPoint=${BASH_REMATCH[4]}${BASH_REMATCH[3]}
			else
				echo "# bad pds2: '$pds2'" 1>&2
				exit 1
			fi
		else
			VPDorDLP="Pointer to Vital Product Data"
			ReservedOrMRTIL="Reserved"
			ReservedOrMRTIL2=""

			Entry_point=" Entry point for INIT function (Legacy)"
		fi
	
		if (( dumppciheader )); then
		(
			printf "\\                   PCI expansion PROM header\n"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+  0))" "PCI magic number (55AA)" "${PciMagicNumber}"
		if (( CodeType == 0 )); then
			printf "\\        %08X: %52s: %s (* 0x200 = 0x%X)\n"    "$((RomStart+  2))" "Image Length in 512 bytes" "${BIOSImageLength}" "$((0x$BIOSImageLength * 512))"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+  3))" "${Entry_point}" "${BIOSData}"
		elif (( CodeType == 3 )); then
			printf "\\        %08X: %52s: %s (* 0x200 = 0x%X)\n"    "$((RomStart+  2))" "EFI initialization size in 512 bytes" "${EFIInitializationSize}" "$((0x$EFIInitializationSize * 512))"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+  4))" "EFI Signature (00000EF1)" "${EFISignature}"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+  8))" "EFI subsystem" "${EFISubSystem}${EFISubSystemString}"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+ 10))" "EFI machine type" "${EFIMachineType}${EFIMachineTypeString}"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+ 12))" "EFI compression type" "${EFICompressionType}${EFICompressionTypeString}"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+ 14))" "EFI reserved (0000000000000000)" "${EFIReserved}"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+ 22))" "EFI offset to image" "${EFIOffsetToImage}"
		else
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+  2))" "Processor architecture unique data" "${ProcessorArchitecture}"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+  4))" "Reserved for processor architecture-unique data" "${ProcessorArchitectureUniqueData}"
		fi
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+ 24))" "Pointer to start of PCI Data Structure" "${PointerPCIDataStructure}"
			printf "\\        %08X: %52s: %s\n"                     "$((RomStart+ 26))" "Pad bytes" "${PadBytes}"
		if [[ -n $varBytes ]]; then
			printf "\\        %8s  %52s:\n%s\n" "" "Variable length pad bytes" "${varBytes}"
		fi
			printf "\\                   PCI Data Structure (4 byte aligned)\n"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+  0))" "Signature string (PCIR)" "${SignatureString}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+  4))" "Vendor ID = config reg 00/01" "${VendorID}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+  6))" "Device ID = config reg 02/03" "${DeviceID}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+  8))" "${VPDorDLP}" "${PointerVitalProductData}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+ 10))" "PCI Data Structure length" "${PCIDataStructureLength}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+ 12))" "PCI Data Structure revision" "${PCIDataStructureRevision}${PCIDataStructureRevisionString}"
			printf "\\        %08X: %52s: %s (Class code / Subclass code / Programming interface code)\n" "$((PCIStart+ 13))" "Class Code = config reg 09/0a/0b" "${ClassCode}"
			printf "\\        %08X: %52s: %s (* 0x200 = 0x%s)\n"    "$((PCIStart+ 16))" "Image Length in 512 bytes" "${ImageLength}" "${ImageBytesHex}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+ 18))" "Revision Level of Code/Data" "${RevisionLevel}"
			printf "\\        %08X: %52s: %s = %s\n"                "$((PCIStart+ 20))" "Code Type" "${CodeType}" "${codeTypeString}"
			printf "\\        %08X: %52s: %s = %s\n"                "$((PCIStart+ 21))" "Indicator" "${Indicator}" "${IndicatorString}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+ 22))" "${ReservedOrMRTIL}" "${MaximumRunTimeImageLength}${ReservedOrMRTIL2}"
                                
		if (( PCIDataStructureRevision >= 3 )); then
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+ 24))" "Pointer to Configuration Utility Code Header" "${PointertoConfigurationUtilityCodeHeader}"
			printf "\\        %08X: %52s: %s\n"                     "$((PCIStart+ 26))" "Pointer to DMTF CLP Entry Point" "${PointertoDMTFCLPEntryPoint}"
		fi
		if [[ -n $varBytes2 ]]; then
			printf "\\        %8s  %52s:\n%s\n" "" "Variable length pad bytes" "${varBytes2}"
		fi
			printf "\nhex\n\n"
	
			if true ; then
				if [[ "${RevisionLevel}" != "0001" ]] ; then
					echo "tokenizer[ ${RevisionLevel} ]tokenizer set-rev-level"
				fi
	
				if [[ "${PointerVitalProductData}" != "0000" ]] ; then
					echo "tokenizer[ ${PointerVitalProductData} ]tokenizer set-vpd-offset"
				fi
	
				if [[ "${ProcessorArchitecture}" != "0034" ]] ; then
					echo "tokenizer[ ${ProcessorArchitecture} ]tokenizer pci-architecture"
				fi
	
				if [[ "${PointerPCIDataStructure}" != "001C" ]] ; then
					echo "tokenizer[ ${PointerPCIDataStructure} ]tokenizer pci-data-structure-start"
				fi
	
				if [[ "${PCIDataStructureLength}" != "0018" ]] ; then
					echo "tokenizer[ ${PCIDataStructureLength} ]tokenizer pci-data-structure-length"
				fi
	
				echo "tokenizer[ ${VendorID} ${DeviceID} ${ClassCode} ]tokenizer pci-header"
	
				echo "tokenizer[ ${ImageBytesHex} ]tokenizer rom-size"
	
				echo
			fi
		) > "${TempFolder}/PCIHeader${part}"
		fi
	else
		printf "# No PCI Header\n" 1>&2
	fi
	
	
	#exit 0
	
	#========================================
	# Dump FCode
	
	sourceFCode="${TempFolder}/FCode${part}"

	printf "# start:%x offset:%x\n" "${RomStart}" "${offset2}" 1>&2

	thechecksum=""
	expectedchecksum=""
	if [ ${offset2} -ne 0 ] ; then
		startOffset=${offset2}
		if ((ImageBytes == 0)); then
			ImageBytes=$offset2
		fi
		xxd -p -r <<< "${romhex:(startOffset)*2:(RomStart + ImageBytes - startOffset)*2}" > "${sourceFCode}"
		if [[ "${codeTypeString}" == "BIOS" ]] ; then
			thechecksum="${romhex:(RomStart + ImageBytes - 1)*2:2}"
			expectedchecksum=$(printf "%02X" $(( ( 0x100 $(perl -pE "s/(..)/-0x\1/g" <<< "${romhex:(RomStart * 2):(ImageBytes - 1)*2}") ) & 0xFF )))
		fi
	else
		startOffset=${RomStart}
		# erase all the trailing FF and 00 except the last one
		cat "${theROM}" > "${sourceFCode}_entire"
		perl -0777 -pE 'substr($_, 0, '${startOffset}') = ""; s/\377+$//; s/\000+$//;' "${theROM}" > "${sourceFCode}"
		if (( $(stat -f "%z" "${sourceFCode}") > 0 )); then
			# if there are some non-FF/00 bytes then keep the first of the trailing 00
			perl -0777 -pE 'substr($_, 0, '${startOffset}') = ""; s/\377+$//; s/\000+$/\000/;' "${theROM}" > "${sourceFCode}"
		fi
	fi

	if (( $(stat -f "%z" "${sourceFCode}") > 0 )); then
		gotpart=1
	else
		gotpart=0
		printf "# No data in this part\n" 1>&2
	fi

	if (( gotpart )); then
		if (( dumppciheader )); then

			if (( numberOfPartsOutput )); then
				printf "\n\n\n"
			fi
			((numberOfPartsOutput++))

			if [[ "${codeTypeString}" == "Open Firmware" ]] ; then
				didDetok=1
				detok -t ${detokoptions} "${sourceFCode}" > "${sourceFCode}.of" 2>> "${detokErrors}"
				echo 1>&2
				perl $script_full_path/ConvertFCodeTokensToForth.pl "${sourceFCode}.of" "${pciHeaderFile}"
			else
				if [[ -n "${pciHeaderFile}" ]] ; then
					cat "${pciHeaderFile}"
				fi
				xxd -u -o ${startOffset} -c 32 -g 4 "${sourceFCode}" | sed -E '/(.*)/s//\\\        \1/'
				if [[ -n "${thechecksum}" ]]; then
					printf "\        %08x: checksum: %s " $((RomStart + ImageBytes - 1)) "${thechecksum}"
					if [[ "${thechecksum}" == "${expectedchecksum}" ]]; then
						printf "(ok)\n"
					else
						printf "(expected %s)\n" "${expectedchecksum}"
					fi
				fi
			fi
		fi

		if (( listparts )); then
			printf "# part:%d type:\"%s\" indicator:\"%s\" offset:%x size:%x\n" "$part" "$codeTypeString" "$IndicatorString" "$RomStart" "$ImageBytes"
		elif (( extractpart == part )); then
			xxd -p -r <<< "${romhex:(RomStart)*2:(PCIStart+ 21 - RomStart)*2}$(printf "%02X" $(( (1-isanother) * 0x80 )) )${romhex:(PCIStart+ 21 + 1)*2:(ImageBytes - (PCIStart+ 21 - RomStart) - 1)*2}"
		fi
	fi

	(( RomStart += ImageBytes ))
	if (( RomStart >= RomFileSize )); then
		break
	fi

	((part++))
done # Loop

# Remove lines that are supposed to occur in the errors output but are very numerous. This makes the other lines (which are likely supposed to be there but are not very numerous) stand out.
if ((didDetok == 1)); then
	printf '\n========================\ndetok log:\n\n' 1>&2
	sed -E "/^Doing Pass 1/,/^Doing Pass 2/ { /# Defined program defined token/ d ; } ; /^Doing Pass 2/,$ { /# Redefined program defined token/ d ; }" "${detokErrors}" 1>&2
fi

exit 0
