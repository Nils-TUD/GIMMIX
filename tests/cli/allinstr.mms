%
% allinstr.mms -- one example of each MMIX instruction
%

		% segmentsizes: 1,0,0,0; pageSize=2^32; r=0x90000; n=0
		LOC		#0
RV		OCTA	#1000200000090000

		LOC		#00090000
		OCTA	#0000000000000007	% PTE  0    (#0000000000000000 .. #00000000FFFFFFFF)


		% forced traps
		LOC		#4000
ATRAP	SWYM	0,0,0				% RESUME 1 @ sp[rT] means doing an IO-op for MMMIX :D
		RESUME	1


		LOC		#1000
		% setup
Main	SETH	$0,#8000
		ORL		$0,RV
		LDOU	$0,$0,0
		PUT		rV,$0

		SET		$0,ATRAP
		ORH		$0,#8000
		PUT		rT,$0
		SET		$6,#1234
		SETML	$24,#0000
		ORL		$24,#BABE
		
		% test resume ropcodes
		GETA	$0,@+32
		PUT		rWW,$0
		SETH	$0,#0000			% resume again
		ORML	$0,#8D00
		ORL		$0,#0100			% LDOI $0,$1,0
		PUT		rXX,$0
		SETH	$1,#8000
		RESUME	1
		
		GETA	$0,@+28
		PUT		rWW,$0
		SETH	$0,#0100			% resume continue
		ORML	$0,#C100
		ORL		$0,#0000			% OR $0,$0,0
		PUT		rXX,$0
		RESUME	1
		
		GETA	$0,@+32
		PUT		rWW,$0
		SETH	$0,#0200			% resume set
		ORML	$0,#0000
		ORL		$0,#0000			% SET $0,rZZ
		PUT		rXX,$0
		PUT		rZZ,#12
		RESUME	1
		
		GETA	$0,@+32
		PUT		rWW,$0
		SETH	$0,#0300			% resume trans -> itc
		ORML	$0,#FD00			% SWYM
		PUT		rXX,$0
		PUT		rYY,0
		PUT		rZZ,0
		RESUME	1
		
		GETA	$0,@+32
		PUT		rWW,$0
		SETH	$0,#0300			% resume trans -> dtc
		ORML	$0,#C100			% OR $0,$0,0
		PUT		rXX,$0
		PUT		rYY,0
		PUT		rZZ,0
		RESUME	1
		
		% remove tc-entrys
		SET		$0,0
		LDVTS	$0,$0,0

		% 0x00
		TRAP	11,6,24
		FCMP	$11,$6,$24
		FUN		$11,$6,$24
		FEQL	$11,$6,$24
		FADD	$11,$6,$24
		FIX		$11,ROUND_OFF,$24
		FSUB	$11,$6,$24
		FIXU	$11,ROUND_NEAR,$24
		FLOT	$11,ROUND_UP,$24
		FLOT	$11,ROUND_DOWN,24
		FLOTU	$11,ROUND_OFF,$24
		FLOTU	$11,ROUND_NEAR,24
		SFLOT	$11,ROUND_UP,$24
		SFLOT	$11,ROUND_DOWN,24
		SFLOTU	$11,ROUND_OFF,$24
		SFLOTU	$11,ROUND_NEAR,24

		% 0x10
		FMUL	$11,$6,$24
		FCMPE	$11,$6,$24
		FUNE	$11,$6,$24
		FEQLE	$11,$6,$24
		FDIV	$11,$6,$24
		FSQRT	$11,ROUND_UP,$24
		FREM	$11,$6,$24
		FINT	$11,ROUND_DOWN,$24
		MUL		$11,$6,$24
		MUL		$11,$6,24
		MULU	$11,$6,$24
		MULU	$11,$6,24
		DIV		$11,$6,$24
		DIV		$11,$6,24
		DIVU	$11,$6,$24
		DIVU	$11,$6,24
	
		% 0x20
		ADD		$11,$6,$24
		ADD		$11,$6,24
		ADDU	$11,$6,$24
		ADDU	$11,$6,24
		SUB		$11,$6,$24
		SUB		$11,$6,24
		SUBU	$11,$6,$24
		SUBU	$11,$6,24
		2ADDU	$11,$6,$24
		2ADDU	$11,$6,24
		4ADDU	$11,$6,$24
		4ADDU	$11,$6,24
		8ADDU	$11,$6,$24
		8ADDU	$11,$6,24
		16ADDU	$11,$6,$24
		16ADDU	$11,$6,24
	
		% 0x30
		CMP		$11,$6,$24
		CMP		$11,$6,24
		CMPU	$11,$6,$24
		CMPU	$11,$6,24
		NEG		$11,6,$24
		NEG		$11,6,24
		NEGU	$11,6,$24
		NEGU	$11,6,24
		SL		$11,$6,$24
		SL		$11,$6,24
		SLU		$11,$6,$24
		SLU		$11,$6,24
		SR		$11,$6,$24
		SR		$11,$6,24
		SRU		$11,$6,$24
		SRU		$11,$6,24
	
		% 0x40
		JMP		4F
1H		JMP		3F
4H		BN		$11,2F
2H		BN		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		BZ		$11,2F
2H		BZ		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		BP		$11,2F
2H		BP		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		BOD		$11,2F
2H		BOD		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		BNN		$11,2F
2H		BNN		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		BNZ		$11,2F
2H		BNZ		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		BNP		$11,2F
2H		BNP		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		BEV		$11,2F
2H		BEV		$11,1B
3H		SWYM	0,0,0
	
	
		% 0x50
		JMP		4F
1H		JMP		3F
4H		PBN		$11,2F
2H		PBN		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		PBZ		$11,2F
2H		PBZ		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		PBP		$11,2F
2H		PBP		$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		PBOD	$11,2F
2H		PBOD	$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		PBNN	$11,2F
2H		PBNN	$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		PBNZ	$11,2F
2H		PBNZ	$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		PBNP	$11,2F
2H		PBNP	$11,1B
3H		SWYM	0,0,0

		JMP		4F
1H		JMP		3F
4H		PBEV	$11,2F
2H		PBEV	$11,1B
3H		SWYM	0,0,0
	
		% 0x60
		CSN		$11,$6,$24
		CSN		$11,$6,24
		CSZ		$11,$6,$24
		CSZ		$11,$6,24
		CSP		$11,$6,$24
		CSP		$11,$6,24
		CSOD	$11,$6,$24
		CSOD	$11,$6,24
		CSNN	$11,$6,$24
		CSNN	$11,$6,24
		CSNZ	$11,$6,$24
		CSNZ	$11,$6,24
		CSNP	$11,$6,$24
		CSNP	$11,$6,24
		CSEV	$11,$6,$24
		CSEV	$11,$6,24
	
		% 0x70
		ZSN		$11,$6,$24
		ZSN		$11,$6,24
		ZSZ		$11,$6,$24
		ZSZ		$11,$6,24
		ZSP		$11,$6,$24
		ZSP		$11,$6,24
		ZSOD	$11,$6,$24
		ZSOD	$11,$6,24
		ZSNN	$11,$6,$24
		ZSNN	$11,$6,24
		ZSNZ	$11,$6,$24
		ZSNZ	$11,$6,24
		ZSNP	$11,$6,$24
		ZSNP	$11,$6,24
		ZSEV	$11,$6,$24
		ZSEV	$11,$6,24
	
		% 0x80
		LDB		$11,$6,$24
		LDB		$11,$6,24
		LDBU	$11,$6,$24
		LDBU	$11,$6,24
		LDW		$11,$6,$24
		LDW		$11,$6,24
		LDWU	$11,$6,$24
		LDWU	$11,$6,24
		LDT		$11,$6,$24
		LDT		$11,$6,24
		LDTU	$11,$6,$24
		LDTU	$11,$6,24
		LDO		$11,$6,$24
		LDO		$11,$6,24
		LDOU	$11,$6,$24
		LDOU	$11,$6,24
	
		% 0x90
		LDSF	$11,$6,$24
		LDSF	$11,$6,24
		LDHT	$11,$6,$24
		LDHT	$11,$6,24
		CSWAP	$11,$6,$24
		CSWAP	$11,$6,24
		LDUNC	$11,$6,$24
		LDUNC	$11,$6,24
		LDVTS	$11,$6,$24
		LDVTS	$11,$6,24
		PRELD	11,$6,$24
		PRELD	11,$6,24
		PREGO	11,$6,$24
		PREGO	11,$6,24
		GETA	$1,1F
		GO		$11,$1,$2
1H		GETA	$1,1F
		GO		$11,$1,0
	
		% 0xA0
1H		STB		$11,$6,$24
		STB		$11,$6,24
		STBU	$11,$6,$24
		STBU	$11,$6,24
		STW		$11,$6,$24
		STW		$11,$6,24
		STWU	$11,$6,$24
		STWU	$11,$6,24
		STT		$11,$6,$24
		STT		$11,$6,24
		STTU	$11,$6,$24
		STTU	$11,$6,24
		STO		$11,$6,$24
		STO		$11,$6,24
		STOU	$11,$6,$24
		STOU	$11,$6,24
	
		% 0xB0
		STSF	$11,$6,$24
		STSF	$11,$6,24
		STHT	$11,$6,$24
		STHT	$11,$6,24
		STCO	11,$6,$24
		STCO	11,$6,24
		STUNC	$11,$6,$24
		STUNC	$11,$6,24
		SYNCD	11,$6,$24
		SYNCD	11,$6,24
		PREST	11,$6,$24
		PREST	11,$6,24
		SYNCID	11,$6,$24
		SYNCID	11,$6,24
		GETA	$1,1F
		PUSHGO	$11,$1,$2
		PUSHGO	$11,$1,0
		JMP		2F

1H		POP		0,0

		% 0xC0
2H		OR		$11,$6,$24
		OR		$11,$6,24
		ORN		$11,$6,$24
		ORN		$11,$6,24
		NOR		$11,$6,$24
		NOR		$11,$6,24
		XOR		$11,$6,$24
		XOR		$11,$6,24
		AND		$11,$6,$24
		AND		$11,$6,24
		ANDN	$11,$6,$24
		ANDN	$11,$6,24
		NAND	$11,$6,$24
		NAND	$11,$6,24
		NXOR	$11,$6,$24
		NXOR	$11,$6,24
	
		% 0xD0
		BDIF	$11,$6,$24
		BDIF	$11,$6,24
		TDIF	$11,$6,$24
		TDIF	$11,$6,24
		WDIF	$11,$6,$24
		WDIF	$11,$6,24
		ODIF	$11,$6,$24
		ODIF	$11,$6,24
		MUX		$11,$6,$24
		MUX		$11,$6,24
		SADD	$11,$6,$24
		SADD	$11,$6,24
		MOR		$11,$6,$24
		MOR		$11,$6,24
		MXOR	$11,$6,$24
		MXOR	$11,$6,24
	
		% 0xE0
		SETH	$11,#A3BE
		SETMH	$11,#A3BE
		SETML	$11,#A3BE
		SETL	$11,#A3BE
		INCH	$11,#A3BE
		INCMH	$11,#A3BE
		INCML	$11,#A3BE
		INCL	$11,#A3BE
		ORH		$11,#A3BE
		ORMH	$11,#A3BE
		ORML	$11,#A3BE
		ORL		$11,#A3BE
		ANDNH	$11,#A3BE
		ANDNMH	$11,#A3BE
		ANDNML	$11,#A3BE
		ANDNL	$11,#A3BE
	
		% 0xF0
		JMP		2F
1H		JMP		3F
		JMP		2F
2H 		JMP		1B
3H		PUSHJ	$11,1F
		JMP		2F
1H		POP		0,0
2H 		PUSHJ	$11,1B
1H		GETA	$11,2F
2H		GETA	$11,1B
		PUT		rP,$11
		PUT		rP,11
		SAVE	$255,0
		UNSAVE	0,$255
		SYNC	0
		SWYM	11,6,24
		GET		$11,rV
		TRIP	11,6,24
		
		% halt cpu
		TRAP	0
