/**
 * $Id: genfsqrttest.c 211 2011-05-15 14:21:51Z nasmussen $
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <fpu_control.h>

#include "common.h"
#include "mmix/io.h"
#include "mmix/error.h"
#include "test/util.h"

#define RESULT_ADDR		0xF000

static uDouble numbers[] = {
	{.d = 0.},
	{.d = -0.},
	{.d = 1.},
	{.d = 2.},
	{.d = -1.5},
	{.d = -0.033123},
	{.d = 1.11},
	{.d = 1.999999},
	{.d = 0x000FFFFFFFFFFFFF},
	{.d = -0.75},
	{.d = -12.75932},
	{.d = 777.114466},
	{.o = 0x0010000000000000},	// min normal
	{.o = 0x000FFFFFFFFFFFFF},	// max subnormal
	{.o = 0x000FF00FF00FF00F},	// another subnormal
	{.o = 0x3FFFFFFFFFFFFFFF},	// 1.9999...
	{.o = 0x7FEFFFFFFFFFFFFF},	// max
	{.o = 0xFFEFFFFFFFFFFFFF},	// min
	{.o = 0x0000000000000001},	// pos min
	{.o = 0x8000000000000001},	// neg max
	{.o = 0x7FF0000000000000},	// +inf
	{.o = 0xFFF0000000000000},	// -inf
	{.o = 0x7FF8000000000000},	// quiet NaN
	{.o = 0x7FF4000000000000},	// signaling NaN
};

int main(int argc,char **argv) {
	if(argc < 2)
		error("Usage: %s (mms|test) <instr> [--ex]\n",argv[0]);

	bool ex = argc > 2 && strcmp(argv[2],"--ex") == 0;

	const sRounding *rounding = getRoundings();
	size_t num = ARRAY_SIZE(numbers);
	if(strcmp(argv[1],"mms") == 0) {
		mprintf("%%\n");
		mprintf("%% fsqrt.mms -- tests fsqrt-instruction (generated)\n");
		mprintf("%%\n");
		mprintf("		LOC		#1000\n");
		mprintf("\n");
		mprintf("Main	OR		$0,$0,$0	%% dummy\n");
		mprintf("		PUT		rA,0\n");
		mprintf("\n");

		mprintf("		%% Put floats in registers\n");
		int reg = 0;
		for(size_t i = 0; i < num; i++) {
			mprintf("		SETH	$%d,#%04OX\n",reg,(numbers[i].o >> 48) & 0xFFFF);
			mprintf("		ORMH	$%d,#%04OX\n",reg,(numbers[i].o >> 32) & 0xFFFF);
			mprintf("		ORML	$%d,#%04OX\n",reg,(numbers[i].o >> 16) & 0xFFFF);
			mprintf("		ORL		$%d,#%04OX\n",reg,(numbers[i].o >>  0) & 0xFFFF);
			mprintf("\n");
			reg++;
		}

		mprintf("		%% Setup location for results\n");
		mprintf("		SETL	$%d,#%X\n",reg,RESULT_ADDR);
		mprintf("\n");
		reg += 2;

		mprintf("		%% Perform tests\n");
		octa loc = RESULT_ADDR;
		for(size_t r = 0; r < ROUND_COUNT; r++) {
			mprintf("		%% Set rounding mode to %s\n",rounding[r].name);
			mprintf("		SETML	$%d,#%X\n",reg - 1,rounding[r].mmix);
			mprintf("		PUT		rA,$%d\n",reg - 1);
			mprintf("\n");

			for(size_t i = 0; i < num; i++) {
				mprintf("		FSQRT	$%d,$%d		%% (sqrt) %e\n",reg,i,numbers[i].d);
				mprintf("		STOU	$%d,$%d,0		%% #%OX\n",reg,reg - 2,loc);
				if(ex) {
					mprintf("		GET		$%d,rA\n",reg);
					mprintf("		STOU	$%d,$%d,8		%% #%OX\n",reg,reg - 2,loc + 8);
					mprintf("		PUT		rA,$%d\n",reg - 1);
				}
				mprintf("		ADDU	$%d,$%d,%d\n",reg - 2,reg - 2,ex ? 16 : 8);
				mprintf("\n");
				loc += ex ? 16 : 8;
			}
		}

		mprintf("		%% Sync memory\n");
		size_t size = ROUND_COUNT * num * (ex ? 16 : 8);
		mprintf("		SETL	$%d,#%X\n",reg - 2,RESULT_ADDR);
		while(size > 0) {
			size_t amount = MIN(0xFE,size);
			mprintf("		SYNCD	#%X,$%d\n",amount,reg - 2);
			mprintf("		ADDU	$%d,$%d,#%X\n",reg - 2,reg - 2,amount + 1);
			size -= amount;
		}
	}
	else {
		// unfortunatly, we can't compare with sqrt, because sqrt seems to ignore the rounding mode.
		// therefore, the results of the MMIX's are different and I have no idea how to do the
		// rounding after the call of sqrt manually :/
		mprintf("m:%X..%X",RESULT_ADDR,
				RESULT_ADDR + ROUND_COUNT * num * (ex ? 16 : 8) - 8);
	}

	return EXIT_SUCCESS;
}
