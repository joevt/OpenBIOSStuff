fcode-version2

external

: test_begin_until
	begin
	until
	begin
		.
		1
	until
	;

: test_begin_while_repeat
	begin
		while
	repeat

	begin
		1
		while
			2
	repeat
	;
	
: test_begin_while_if
	begin
		while
			begin
				while
					if
					then
			repeat
	repeat
	;


: test_if_then
	if
		then
	1
	if
		2
	then
	;


: test_if_else_then
	if
	else
	then

	1
	if
		2
	else
		3
	then
	;

: test_begin_again
	begin
	again

	begin
		1
	again
	;

: test_begin_if_else_again
	begin
		if
		else
			again
		then


	begin
		1
		if
			2
		else
			3
			again
		then
	;


: test_begin_if_again
	begin
		if
			again
		then

	begin
		1
		if
			1
			again
		then
	;


: test_begin_if__again
	begin
		1
		if
			1
			again
			1
		then
		1
	;


: test_begin_if_if_again
	begin
		if
			if
				again
			then
		then


	begin
		1
		if
			1
			if
				1
				again
			then
		then
	;


: test_case_of_endof_endcase
	case
		of
		endof
		of
		endof
	endcase

	case
		1 of
			2
		endof
		2 of
			3
		endof
		( default )
			4
	endcase
	;
	

fcode-end
