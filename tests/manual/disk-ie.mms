%
% disk-ie.mms -- tests the disk-device with interrupts enabled
%

		LOC		#0
START	BYTE	"Waiting until disk is ready...",0
READY	BYTE	"done",#d,#a,0
CAP		BYTE	"Capacity: ",0
CAPEND	BYTE	" bytes",#d,#a,0
READ1	BYTE	"Reading content...",#d,#a,"------",#d,#a,0
WRITE	BYTE	"Overwriting content...",#d,#a,0
READ2	BYTE	"Reading again...",#d,#a,"------",#d,#a,0
END		BYTE	#d,#a,"------",#d,#a,0
REWIND	BYTE	"Rewinding content...",#d,#a,0


		% dynamic traps
		LOC		#600000
DTRAP	GET		$254,rQ
		PUT		rQ,0
		SET		$254,1
		SETH	$255,#FFFF
		ORMH	$255,#FF00
		RESUME	1
		
		
		LOC		#1000

Main	PUT		rG,254
		% setup rTT
		SETH	$0,#8000
		ORMH	$0,DTRAP>>32
		ORML	$0,DTRAP>>16
		ORL		$0,DTRAP>>0
		PUT		rTT,$0
		
		SET		$1,START
		PUSHJ	$0,putmsg
		
		% enable interrupts
		NEG		$0,0,1
		ANDNMH	$0,#0001
		PUT		rK,$0
		
		% wait until ready-bit is set
		SETH	$0,#8003
1H		LDOU	$1,$0,0
		AND		$1,$1,#20
		PBZ		$1,1B
		
		% print ready & capacity
		SET		$1,READY
		PUSHJ	$0,putmsg
		SET		$1,CAP
		PUSHJ	$0,putmsg
		
		% print capacity
		SETH	$0,#8003
		SET		$1,0
		LDOU	$2,$0,24
		SLU		$2,$2,9		% *= sector-size
		SET		$3,10
		PUSHJ	$0,io:putu
		
		SET		$1,CAPEND
		PUSHJ	$0,putmsg
		SET		$1,READ1
		PUSHJ	$0,putmsg
		
		% read sector
		SET		$1,0
		SET		$2,#F000
		ORH		$2,#8000
		PUSHJ	$0,readSec
		
		% print content
		SET		$1,#F000
		PUSHJ	$0,putmsg
		SET		$1,END
		PUSHJ	$0,putmsg
		
		SET		$1,WRITE
		PUSHJ	$0,putmsg
		
		% backup content
		SET		$1,#E000
		ORH		$1,#8000
		SET		$2,#F000
		ORH		$2,#8000
		PUSHJ	$0,str:copy
		
		% replace content
		SET		$1,#F000
		ORH		$1,#8000
		SET		$2,START
		ORH		$2,#8000
		PUSHJ	$0,str:copy
		
		% write sector
		SET		$1,0
		SET		$2,#F000
		ORH		$2,#8000
		PUSHJ	$0,wrtSec
		
		% print msg READ2
		SET		$1,READ2
		PUSHJ	$0,putmsg
		
		% read sector
		SET		$1,0
		SET		$2,#F000
		ORH		$2,#8000
		PUSHJ	$0,readSec
		
		% print content
		SET		$1,#F000
		PUSHJ	$0,putmsg
		SET		$1,END
		PUSHJ	$0,putmsg
		
		SET		$1,REWIND
		PUSHJ	$0,putmsg
		
		% write sector
		SET		$1,0
		SET		$2,#E000
		ORH		$2,#8000
		PUSHJ	$0,wrtSec
		

loop	JMP		loop

% void putmsg(char *msg)
putmsg	GET		$1,rJ
		SET		$3,0
		SET		$4,$0
		ORH		$4,#8000
		PUSHJ	$2,io:puts
		PUT		rJ,$1
		POP		0,0

% void readSec(octa sec,octa *buf)
readSec	GET		$2,rJ				% save rJ
		SETH	$3,#8003			% disk-address
		STCO	1,$3,#8				% sector-count = 1
		STOU	$0,$3,#16			% sector-number = sec
		STCO	#3,$3,#0			% start read-command with interrupts enabled
		SET		$254,0				% first, reset our flag-register
_waitr	PBZ		$254,_waitr			% wait here until the interrupt-handler sets the flag
		% now read one sector from the disk-buffer into buf
		SET		$4,0
		INCML	$3,#0008			% address of disk-buffer
2H		LDOU	$5,$3,$4
		STOU	$5,$1,$4
		ADDU	$4,$4,8				% next octa
		CMPU	$5,$4,64			% 64*8 = 512
		PBNZ	$5,2B
		PUT		rJ,$2				% restore rJ
		POP		0,0

% void wrtSec(octa sec,octa *buf)
wrtSec	GET		$2,rJ				% save rJ
		SETH	$3,#8003			% disk-address
		% first, write it to disk-buffer
		SET		$4,0
		INCML	$3,#0008			% address of disk-buffer
1H		LDOU	$5,$1,$4
		STOU	$5,$3,$4
		ADDU	$4,$4,8				% next octa
		CMPU	$5,$4,64			% 64*8 = 512
		PBNZ	$5,1B
		% now, tell the device it should write it to disk
		SETH	$3,#8003			% disk-address
		STCO	1,$3,#8				% sector-count = 1
		STOU	$0,$3,#16			% sector-number = sec
		STCO	#7,$3,#0			% start write-command with interrupts enabled
		SET		$254,0				% first, reset our flag-register
_waitw	PBZ		$254,_waitw			% wait here until the interrupt-handler sets the flag
		PUT		rJ,$2				% restore rJ
		POP		0,0


#include "string.mmi"
#include "io.mmi"
