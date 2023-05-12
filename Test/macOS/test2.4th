fcode-version2
\ format:    0x08
\ checksum:  0xc419 (ok)
\ len:       0x1f0 (496 bytes)
hex




external
: test_begin_until																\ (800)
	begin
	until																		\ (0xffff)
	begin
		.
		1
	until																		\ (0xfffd)
	;


: test_begin_while_repeat														\ (801)
	begin
		while																	\ (0x6)
	repeat																		\ (0xfffc)
	begin
		1
		while																	\ (0x7)
			2
	repeat																		\ (0xfffa)
	;


: test_begin_while_if															\ (802)
	begin
		while																	\ (0x12)
			begin
				while															\ (0xa)
					if															\ (0x3)
					then
			repeat																\ (0xfff8)
	repeat																		\ (0xfff0)
	;


: test_if_then																	\ (803)
	if																			\ (0x3)
	then
	1
	if																			\ (0x4)
		2
	then
	;


: test_if_else_then																\ (804)
	if																			\ (0x6)
	else																		\ (0x4)
	then
	1
	if																			\ (0x7)
		2
	else																		\ (0x5)
		3
	then
	;


: test_begin_again																\ (805)
	begin
	again																		\ (0xffff)
	begin
		1
	again																		\ (0xfffe)
	;


: test_begin_if_else_again														\ (806)
	begin
		if																		\ (0x6)
		else																	\ (0x7)
		again																	\ (0xfff8)
	then
	begin
		1
		if																		\ (0x7)
			2
		else																	\ (0x8)
			3
		again																	\ (0xfff5)
	then
	;


: test_begin_if_again															\ (807)
	begin
		while																	\ (0x6)
	repeat																		\ (0xfffc)
	begin
		1
		while																	\ (0x7)
			1
	repeat																		\ (0xfffa)
	;


: test_begin_if__again															\ (808)
	begin
		1
		if																		\ (0x8)
			1
		again																	\ (0xfffa)
		1
	then
	1
	;


: test_begin_if_if_again														\ (809)
	begin
		if																		\ (0xa)
			while																\ (0x6)
		repeat																	\ (0xfff9)
	then
	begin
		1
		if																		\ (0xc)
			1
			while																\ (0x7)
				1
		repeat																	\ (0xfff6)
	then
	;


: test_case_of_endof_endcase													\ (80a)
	case
		of																		\ (0x5)
		endof																	\ (0x9)
		of																		\ (0x5)
		endof																	\ (0x3)
	endcase
	case
		1
		of																		\ (0x6)
			2
		endof																	\ (0x10)
		2
		of																		\ (0x6)
			3
		endof																	\ (0x8)
		4
	endcase
	;


fcode-end
\ detokenizing finished after 496 of 496 bytes.

