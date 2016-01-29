/*
 * display.c -- display controller simulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include "common.h"
#include "core/cpu.h"
#include "core/register.h"
#include "dev/timer.h"
#include "dev/dspkbd.h"
#include "mmix/io.h"
#include "exception.h"
#include "config.h"
#include "sim.h"

#define KEYBOARD_CTRL		0	/* keyboard control register */
#define KEYBOARD_DATA		8	/* keyboard data register */

#define KEYBOARD_RDY		0x01	/* keyboard has a character */
#define KEYBOARD_IEN		0x02	/* enable keyboard interrupt */

#define WINDOW_SIZE_X		640
#define WINDOW_SIZE_Y		480
#define WINDOW_POS_X		1680
#define WINDOW_POS_Y		400

#define WIN_REFRESH_INT		(30 * 1000 * 1000)	/* window refresh interval in nanoseconds */

#define C2B(c,ch)		(((((c) & 0xFF) * ch.scale) >> 8) * ch.factor)
// the first 8 bit specify the alpha-value
#define RGB2PIXEL(r,g,b)	((0xFF << 24) | \
				 C2B(r, vga.red) | \
				 C2B(g, vga.green) | \
				 C2B(b, vga.blue))

#define CELL_SIZE_X	8
#define CELL_SIZE_Y	16

#define TEXT_SIZE_X	(WINDOW_SIZE_X / CELL_SIZE_X)
#define TEXT_SIZE_Y	(WINDOW_SIZE_Y / CELL_SIZE_Y)

#define MAX_MAKE	2
#define MAX_BREAK	3

#define KBD_BUF_MAX		100
#define KBD_BUF_NEXT(p)		(((p) + 1) % KBD_BUF_MAX)

typedef struct {
	unsigned long scale;
	unsigned long factor;
} ColorChannel;

typedef struct {
	int argc;
	char **argv;
	Display *display;
	Window win;
	GC gc;
	XImage *image;
	ColorChannel red,green,blue;
	XExposeEvent expose;
	XClientMessageEvent shutdown;
} VGA;

typedef struct {
	unsigned int xKeycode;
	int pcNumMake;
	byte pcKeyMake[MAX_MAKE];
	int pcNumBreak;
	byte pcKeyBreak[MAX_BREAK];
} Keycode;

static void blinkScreen(void);

static void keyPressed(unsigned int xKeycode);
static void keyReleased(unsigned int xKeycode);

static octa displayRead(octa addr,bool sideEffects);
static void displayWrite(octa addr,octa data);
static void displayReset(void);
static void displayInit(void);
static void displayExit(void);

static octa keyboardRead(octa addr,bool sideEffects);
static void keyboardWrite(octa addr,octa data);
static void keyboardReset(void);
static void keyboardInit(void);
static void keyboardExit(void);

static bool debug = false;
static bool debugKeycode = false;
static volatile bool installed = false;

static bool run;
static VGA vga;

static bool refreshRunning = false;
static bool blink = false;

static int myArgc = 1;
static const char *myArgv[] = {"GIMMIX",NULL};

static wyde text[TEXT_SIZE_Y][TEXT_SIZE_X];

static const byte font[] = {
#include "font"
};

static const char *screen[] = {
#include "screen"
};

static Keycode kbdCodeTbl[] = {
	{ 0x09, 1, { 0x76, 0x00 }, 2, { 0xF0, 0x76, 0x00 } },
	{ 0x43, 1, { 0x05, 0x00 }, 2, { 0xF0, 0x05, 0x00 } },
	{ 0x44, 1, { 0x06, 0x00 }, 2, { 0xF0, 0x06, 0x00 } },
	{ 0x45, 1, { 0x04, 0x00 }, 2, { 0xF0, 0x04, 0x00 } },
	{ 0x46, 1, { 0x0C, 0x00 }, 2, { 0xF0, 0x0C, 0x00 } },
	{ 0x47, 1, { 0x03, 0x00 }, 2, { 0xF0, 0x03, 0x00 } },
	{ 0x48, 1, { 0x0B, 0x00 }, 2, { 0xF0, 0x0B, 0x00 } },
	{ 0x49, 1, { 0x83, 0x00 }, 2, { 0xF0, 0x83, 0x00 } },
	{ 0x4A, 1, { 0x0A, 0x00 }, 2, { 0xF0, 0x0A, 0x00 } },
	{ 0x4B, 1, { 0x01, 0x00 }, 2, { 0xF0, 0x01, 0x00 } },
	{ 0x4C, 1, { 0x09, 0x00 }, 2, { 0xF0, 0x09, 0x00 } },
	{ 0x5F, 1, { 0x78, 0x00 }, 2, { 0xF0, 0x78, 0x00 } },
	{ 0x60, 1, { 0x07, 0x00 }, 2, { 0xF0, 0x07, 0x00 } },
	/*------------------------------------------------*/
	{ 0x31, 1, { 0x0E, 0x00 }, 2, { 0xF0, 0x0E, 0x00 } },
	{ 0x0A, 1, { 0x16, 0x00 }, 2, { 0xF0, 0x16, 0x00 } },
	{ 0x0B, 1, { 0x1E, 0x00 }, 2, { 0xF0, 0x1E, 0x00 } },
	{ 0x0C, 1, { 0x26, 0x00 }, 2, { 0xF0, 0x26, 0x00 } },
	{ 0x0D, 1, { 0x25, 0x00 }, 2, { 0xF0, 0x25, 0x00 } },
	{ 0x0E, 1, { 0x2E, 0x00 }, 2, { 0xF0, 0x2E, 0x00 } },
	{ 0x0F, 1, { 0x36, 0x00 }, 2, { 0xF0, 0x36, 0x00 } },
	{ 0x10, 1, { 0x3D, 0x00 }, 2, { 0xF0, 0x3D, 0x00 } },
	{ 0x11, 1, { 0x3E, 0x00 }, 2, { 0xF0, 0x3E, 0x00 } },
	{ 0x12, 1, { 0x46, 0x00 }, 2, { 0xF0, 0x46, 0x00 } },
	{ 0x13, 1, { 0x45, 0x00 }, 2, { 0xF0, 0x45, 0x00 } },
	{ 0x14, 1, { 0x4E, 0x00 }, 2, { 0xF0, 0x4E, 0x00 } },
	{ 0x15, 1, { 0x55, 0x00 }, 2, { 0xF0, 0x55, 0x00 } },
	{ 0x16, 1, { 0x66, 0x00 }, 2, { 0xF0, 0x66, 0x00 } },
	/*------------------------------------------------*/
	{ 0x17, 1, { 0x0D, 0x00 }, 2, { 0xF0, 0x0D, 0x00 } },
	{ 0x18, 1, { 0x15, 0x00 }, 2, { 0xF0, 0x15, 0x00 } },
	{ 0x19, 1, { 0x1D, 0x00 }, 2, { 0xF0, 0x1D, 0x00 } },
	{ 0x1A, 1, { 0x24, 0x00 }, 2, { 0xF0, 0x24, 0x00 } },
	{ 0x1B, 1, { 0x2D, 0x00 }, 2, { 0xF0, 0x2D, 0x00 } },
	{ 0x1C, 1, { 0x2C, 0x00 }, 2, { 0xF0, 0x2C, 0x00 } },
	{ 0x1D, 1, { 0x35, 0x00 }, 2, { 0xF0, 0x35, 0x00 } },
	{ 0x1E, 1, { 0x3C, 0x00 }, 2, { 0xF0, 0x3C, 0x00 } },
	{ 0x1F, 1, { 0x43, 0x00 }, 2, { 0xF0, 0x43, 0x00 } },
	{ 0x20, 1, { 0x44, 0x00 }, 2, { 0xF0, 0x44, 0x00 } },
	{ 0x21, 1, { 0x4D, 0x00 }, 2, { 0xF0, 0x4D, 0x00 } },
	{ 0x22, 1, { 0x54, 0x00 }, 2, { 0xF0, 0x54, 0x00 } },
	{ 0x23, 1, { 0x5B, 0x00 }, 2, { 0xF0, 0x5B, 0x00 } },
	{ 0x24, 1, { 0x5A, 0x00 }, 2, { 0xF0, 0x5A, 0x00 } },
	/*------------------------------------------------*/
	{ 0x42, 1, { 0x58, 0x00 }, 2, { 0xF0, 0x58, 0x00 } },
	{ 0x26, 1, { 0x1C, 0x00 }, 2, { 0xF0, 0x1C, 0x00 } },
	{ 0x27, 1, { 0x1B, 0x00 }, 2, { 0xF0, 0x1B, 0x00 } },
	{ 0x28, 1, { 0x23, 0x00 }, 2, { 0xF0, 0x23, 0x00 } },
	{ 0x29, 1, { 0x2B, 0x00 }, 2, { 0xF0, 0x2B, 0x00 } },
	{ 0x2A, 1, { 0x34, 0x00 }, 2, { 0xF0, 0x34, 0x00 } },
	{ 0x2B, 1, { 0x33, 0x00 }, 2, { 0xF0, 0x33, 0x00 } },
	{ 0x2C, 1, { 0x3B, 0x00 }, 2, { 0xF0, 0x3B, 0x00 } },
	{ 0x2D, 1, { 0x42, 0x00 }, 2, { 0xF0, 0x42, 0x00 } },
	{ 0x2E, 1, { 0x4B, 0x00 }, 2, { 0xF0, 0x4B, 0x00 } },
	{ 0x2F, 1, { 0x4C, 0x00 }, 2, { 0xF0, 0x4C, 0x00 } },
	{ 0x30, 1, { 0x52, 0x00 }, 2, { 0xF0, 0x52, 0x00 } },
	{ 0x33, 1, { 0x5D, 0x00 }, 2, { 0xF0, 0x5D, 0x00 } },
	/*------------------------------------------------*/
	{ 0x32, 1, { 0x12, 0x00 }, 2, { 0xF0, 0x12, 0x00 } },
	{ 0x5E, 1, { 0x61, 0x00 }, 2, { 0xF0, 0x61, 0x00 } },
	{ 0x34, 1, { 0x1A, 0x00 }, 2, { 0xF0, 0x1A, 0x00 } },
	{ 0x35, 1, { 0x22, 0x00 }, 2, { 0xF0, 0x22, 0x00 } },
	{ 0x36, 1, { 0x21, 0x00 }, 2, { 0xF0, 0x21, 0x00 } },
	{ 0x37, 1, { 0x2A, 0x00 }, 2, { 0xF0, 0x2A, 0x00 } },
	{ 0x38, 1, { 0x32, 0x00 }, 2, { 0xF0, 0x32, 0x00 } },
	{ 0x39, 1, { 0x31, 0x00 }, 2, { 0xF0, 0x31, 0x00 } },
	{ 0x3A, 1, { 0x3A, 0x00 }, 2, { 0xF0, 0x3A, 0x00 } },
	{ 0x3B, 1, { 0x41, 0x00 }, 2, { 0xF0, 0x41, 0x00 } },
	{ 0x3C, 1, { 0x49, 0x00 }, 2, { 0xF0, 0x49, 0x00 } },
	{ 0x3D, 1, { 0x4A, 0x00 }, 2, { 0xF0, 0x4A, 0x00 } },
	{ 0x3E, 1, { 0x59, 0x00 }, 2, { 0xF0, 0x59, 0x00 } },
	/*------------------------------------------------*/
	{ 0x25, 1, { 0x14, 0x00 }, 2, { 0xF0, 0x14, 0x00 } },
	{ 0x73, 2, { 0xE0, 0x69 }, 3, { 0xE0, 0xF0, 0x69 } },
	{ 0x40, 1, { 0x11, 0x00 }, 2, { 0xF0, 0x11, 0x00 } },
	{ 0x41, 1, { 0x29, 0x00 }, 2, { 0xF0, 0x29, 0x00 } },
	{ 0x71, 2, { 0xE0, 0x6B }, 3, { 0xE0, 0xF0, 0x6B } },
	{ 0x74, 2, { 0xE0, 0x72 }, 3, { 0xE0, 0xF0, 0x72 } },
	{ 0x75, 2, { 0xE0, 0x7A }, 3, { 0xE0, 0xF0, 0x7A } },
	{ 0x6D, 2, { 0xE0, 0x14 }, 3, { 0xE0, 0xF0, 0x14 } },
	/*------------------------------------------------*/
	{ 0x6A, 2, { 0xE0, 0x4A }, 3, { 0xE0, 0xF0, 0x4A } },
	{ 0x61, 2, { 0xE0, 0x6C }, 3, { 0xE0, 0xF0, 0x6C } },
	{ 0x63, 2, { 0xE0, 0x7D }, 3, { 0xE0, 0xF0, 0x7D } },
	{ 0x6B, 2, { 0xE0, 0x71 }, 3, { 0xE0, 0xF0, 0x71 } },
	{ 0x67, 2, { 0xE0, 0x69 }, 3, { 0xE0, 0xF0, 0x69 } },
	{ 0x69, 2, { 0xE0, 0x14 }, 3, { 0xE0, 0xF0, 0x14 } },
	{ 0x62, 2, { 0xE0, 0x75 }, 3, { 0xE0, 0xF0, 0x75 } },
	{ 0x64, 2, { 0xE0, 0x6B }, 3, { 0xE0, 0xF0, 0x6B } },
	{ 0x68, 2, { 0xE0, 0x5A }, 3, { 0xE0, 0xF0, 0x5A } },
	{ 0x66, 2, { 0xE0, 0x74 }, 3, { 0xE0, 0xF0, 0x74 } },
	/*------------------------------------------------*/
	{ 0x4D, 1, { 0x77, 0x00 }, 2, { 0xF0, 0x77, 0x00 } },
	{ 0x70, 2, { 0xE0, 0x7D }, 3, { 0xE0, 0xF0, 0x7D } },
	{ 0x3F, 1, { 0x7C, 0x00 }, 2, { 0xF0, 0x7C, 0x00 } },
	{ 0x52, 1, { 0x7B, 0x00 }, 2, { 0xF0, 0x7B, 0x00 } },
	{ 0x4F, 1, { 0x6C, 0x00 }, 2, { 0xF0, 0x6C, 0x00 } },
	{ 0x50, 1, { 0x75, 0x00 }, 2, { 0xF0, 0x75, 0x00 } },
	{ 0x51, 1, { 0x7D, 0x00 }, 2, { 0xF0, 0x7D, 0x00 } },
	{ 0x56, 1, { 0x79, 0x00 }, 2, { 0xF0, 0x79, 0x00 } },
	{ 0x53, 1, { 0x6B, 0x00 }, 2, { 0xF0, 0x6B, 0x00 } },
	{ 0x54, 1, { 0x73, 0x00 }, 2, { 0xF0, 0x73, 0x00 } },
	{ 0x55, 1, { 0x74, 0x00 }, 2, { 0xF0, 0x74, 0x00 } },
	{ 0x57, 1, { 0x69, 0x00 }, 2, { 0xF0, 0x69, 0x00 } },
	{ 0x58, 1, { 0x72, 0x00 }, 2, { 0xF0, 0x72, 0x00 } },
	{ 0x59, 1, { 0x7A, 0x00 }, 2, { 0xF0, 0x7A, 0x00 } },
	{ 0x6C, 2, { 0xE0, 0x11 }, 3, { 0xE0, 0xF0, 0x11 } },
	{ 0x5A, 1, { 0x70, 0x00 }, 2, { 0xF0, 0x70, 0x00 } },
	{ 0x5B, 1, { 0x71, 0x00 }, 2, { 0xF0, 0x71, 0x00 } },
	/*------------------------------------------------*/
	{ 0x72, 2, { 0xE0, 0x74 }, 3, { 0xE0, 0xF0, 0x74 } },	/* arrow down */
	{ 0x6F, 2, { 0xE0, 0x75 }, 3, { 0xE0, 0xF0, 0x75 } },	/* arrow left */
	{ 0x76, 2, { 0xE0, 0x70 }, 3, { 0xE0, 0xF0, 0x70 } },	/* insert */
	{ 0x6E, 2, { 0xE0, 0x6C }, 3, { 0xE0, 0xF0, 0x6C } },	/* pos1 */
	{ 0x77, 2, { 0xE0, 0x71 }, 3, { 0xE0, 0xF0, 0x71 } },	/* delete */
	{ 0x85, 2, { 0xE0, 0x1F }, 3, { 0xE0, 0xF0, 0x1F } },	/* left super */
	{ 0x86, 2, { 0xE0, 0x27 }, 3, { 0xE0, 0xF0, 0x27 } },	/* right super */
	{ 0x87, 2, { 0xE0, 0x2F }, 3, { 0xE0, 0xF0, 0x2F } },	/* apps */
};

static byte kbdBuf[KBD_BUF_MAX];
static int kbdBufWritePtr;
static int kbdBufReadPtr;

static octa kbdCtrl;
static octa kbdData;

static sDevice displayDev;
static sDevice kbDev;

void dspkbd_init(void) {
	if(cfg_useDspKbd()) {
		// register devices
		displayDev.name = "Display";
		displayDev.startAddr = DISPLAY_START_ADDR;
		displayDev.endAddr = DISPLAY_START_ADDR + TEXT_SIZE_Y * 128 * 8 - 1;
		displayDev.irqMask = 0;
		displayDev.reset = displayReset;
		displayDev.read = displayRead;
		displayDev.write = displayWrite;
		displayDev.shutdown = displayExit;
		bus_register(&displayDev);

		kbDev.name = "Keyboard";
		kbDev.startAddr = KEYBOARD_START_ADDR;
		kbDev.endAddr = KEYBOARD_START_ADDR + 2 * sizeof(octa) - 1;
		kbDev.irqMask = (octa)1 << KEYBOARD_IRQ;
		kbDev.reset = keyboardReset;
		kbDev.read = keyboardRead;
		kbDev.write = keyboardWrite;
		kbDev.shutdown = keyboardExit;
		bus_register(&kbDev);

		displayInit();
		keyboardInit();
	}
}

/**************************************************************/
/**************************************************************/

/* monitor server */

static ColorChannel mask2channel(unsigned long mask) {
	unsigned long f;
	ColorChannel ch;

	if(mask == 0) {
		sim_error("color mask is 0 in mask2channel");
	}
	for(f = 1; (mask & 1) == 0; f <<= 1) {
		mask >>= 1;
	}
	ch.factor = f;
	ch.scale = mask + 1;
	while((mask & 1) != 0) {
		mask >>= 1;
	}
	if(mask != 0) {
		sim_error("scattered color mask bits in mask2channel");
	}
	return ch;
}

static void initMonitor(int argc,char *argv[]) {
	int screenNum;
	Window rootWin;
	XVisualInfo visualTemp;
	XVisualInfo *visualInfo;
	int visualCount;
	int bestMatch;
	int bestDepth;
	Visual *visual;
	int i;
	unsigned long pixel;
	int x,y;
	Colormap colormap;
	XSetWindowAttributes attrib;
	XSizeHints *sizeHints;
	XWMHints *wmHints;
	XClassHint *classHints;
	XTextProperty windowName;
	XGCValues gcValues;

	/* connect to X server */
	if(XInitThreads() == 0) {
		sim_error("no thread support for X11");
	}
	vga.display = XOpenDisplay(NULL);
	if(vga.display == NULL) {
		sim_error("cannot connect to X server");
	}
	screenNum = DefaultScreen(vga.display);
	rootWin = RootWindow(vga.display, screenNum);
	/* find TrueColor visual */
	visualTemp.screen = screenNum;
	visualTemp.class = TrueColor;
	visualInfo = XGetVisualInfo(vga.display,VisualClassMask | VisualScreenMask,&visualTemp,
			&visualCount);
	if(visualInfo == NULL || visualCount == 0) {
		sim_error("no TrueColor visual found");
	}
	bestMatch = 0;
	bestDepth = visualInfo[0].depth;
	visual = visualInfo[0].visual;
	for(i = 1; i < visualCount; i++) {
		if(visualInfo[i].depth > bestDepth) {
			bestMatch = i;
			bestDepth = visualInfo[i].depth;
			visual = visualInfo[i].visual;
		}
	}
	/* build color channels */
	vga.red = mask2channel(visualInfo[bestMatch].red_mask);
	vga.green = mask2channel(visualInfo[bestMatch].green_mask);
	vga.blue = mask2channel(visualInfo[bestMatch].blue_mask);
	/* create and initialize image */
	vga.image = XCreateImage(vga.display,visual,bestDepth,ZPixmap,0,NULL,WINDOW_SIZE_X,
			WINDOW_SIZE_Y,32,0);
	if(vga.image == NULL) {
		sim_error("cannot allocate image");
	}
	vga.image->data = malloc(vga.image->height * vga.image->bytes_per_line);
	if(vga.image->data == NULL) {
		sim_error("cannot allocate image memory");
	}
	pixel = RGB2PIXEL(0, 0, 0);
	for(y = 0; y < WINDOW_SIZE_Y; y++) {
		for(x = 0; x < WINDOW_SIZE_X; x++) {
			XPutPixel(vga.image, x, y, pixel);
		}
	}
	/* allocate a colormap */
	colormap = XCreateColormap(vga.display,rootWin,visual,AllocNone);
	/* create the window */
	attrib.colormap = colormap;
	attrib.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask;
	attrib.background_pixel = RGB2PIXEL(0, 0, 0);
	attrib.border_pixel = RGB2PIXEL(0, 0, 0);
	vga.win = XCreateWindow(vga.display,rootWin,WINDOW_POS_X,WINDOW_POS_Y,WINDOW_SIZE_X,
			WINDOW_SIZE_Y,0,bestDepth,InputOutput,visual,CWEventMask | CWColormap | CWBackPixel
					| CWBorderPixel,&attrib);
	/* give hints to window manager */
	sizeHints = XAllocSizeHints();
	wmHints = XAllocWMHints();
	classHints = XAllocClassHint();
	if(sizeHints == NULL || wmHints == NULL || classHints == NULL) {
		sim_error("hint allocation failed");
	}
	sizeHints->flags = PMinSize | PMaxSize;
	sizeHints->min_width = WINDOW_SIZE_X;
	sizeHints->min_height = WINDOW_SIZE_Y;
	sizeHints->max_width = WINDOW_SIZE_X;
	sizeHints->max_height = WINDOW_SIZE_Y;
	wmHints->flags = StateHint | InputHint;
	wmHints->input = True;
	wmHints->initial_state = NormalState;
	classHints->res_name = (char*)"GIMMIX";
	classHints->res_class = (char*)"GIMMIX";
	if(XStringListToTextProperty(&classHints->res_name,1,&windowName) == 0) {
		sim_error("property allocation failed");
	}
	XSetWMProperties(vga.display,vga.win,&windowName,NULL,argv,argc,sizeHints,wmHints,classHints);
	/* create a GC */
	vga.gc = XCreateGC(vga.display,vga.win,0,&gcValues);
	/* finally get the window displayed */
	XMapWindow(vga.display,vga.win);
	/* prepare expose event */
	vga.expose.type = Expose;
	vga.expose.display = vga.display;
	vga.expose.window = vga.win;
	vga.expose.x = 0;
	vga.expose.y = 0;
	vga.expose.width = WINDOW_SIZE_X;
	vga.expose.height = WINDOW_SIZE_Y;
	vga.expose.count = 0;
	/* prepare shutdown event */
	vga.shutdown.type = ClientMessage;
	vga.shutdown.display = vga.display;
	vga.shutdown.window = vga.win;
	vga.shutdown.message_type = XA_WM_COMMAND;
	vga.shutdown.format = 32;
	vga.shutdown.data.l[0] = 0xDEADBEEF;
	/* move the window to the desired position (many window-manager ignore the position given by
	 * XCreateWindow) */
	XMoveWindow(vga.display,vga.win,WINDOW_POS_X,WINDOW_POS_Y);
	/* say that the graphics controller is installed */
	XSync(vga.display,False);
	installed = true;
}

static void exitMonitor(void) {
	XFreeGC(vga.display,vga.gc);
	XUnmapWindow(vga.display,vga.win);
	XDestroyWindow(vga.display,vga.win);
	XDestroyImage(vga.image);
	XCloseDisplay(vga.display);
	installed = false;
}

static int ioErrorHandler(Display *display) {
	UNUSED(display);
	sim_error("connection to monitor window lost");
	/* never reached */
	return 0;
}

static void *server(void *ignore) {
	UNUSED(ignore);
	XEvent event;

	initMonitor(vga.argc,vga.argv);
	XSetIOErrorHandler(ioErrorHandler);
	run = true;
	while(run) {
		XNextEvent(vga.display,&event);
		switch(event.type) {
		case Expose:
			blinkScreen();
			XPutImage(vga.display,vga.win,vga.gc,vga.image,event.xexpose.x,event.xexpose.y,
					event.xexpose.x,event.xexpose.y,event.xexpose.width,event.xexpose.height);
			break;
		case ClientMessage:
			if(event.xclient.message_type == XA_WM_COMMAND && event.xclient.format == 32
					&& event.xclient.data.l[0] == 0xDEADBEEF) {
				run = false;
			}
			break;
		case KeyPress:
			keyPressed(event.xkey.keycode);
			break;
		case KeyRelease:
			keyReleased(event.xkey.keycode);
			break;
		default:
			break;
		}
	}
	exitMonitor();
	return NULL;
}

/**************************************************************/

/* refresh timer */

static void *refresh(void *ignore) {
	UNUSED(ignore);
	static int blinkCounter = 0;
	struct timespec delay;

	while(refreshRunning) {
		if(++blinkCounter == 5) {
			blinkCounter = 0;
			blink = !blink;
		}
		XSendEvent(vga.display,vga.win,False,0,(XEvent *)&vga.expose);
		XFlush(vga.display);
		delay.tv_sec = 0;
		delay.tv_nsec = WIN_REFRESH_INT;
		nanosleep(&delay,&delay);
	}
	return NULL;
}

/**************************************************************/

/* server interface */

static void vgaInit(void) {
	pthread_attr_t attr;
	pthread_t thread;

	/* start monitor server in a separate thread */
	vga.argc = myArgc;
	vga.argv = (char**)myArgv;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	if(pthread_create(&thread,&attr,server,NULL) != 0) {
		sim_error("cannot start monitor server");
	}
	while(!installed) /*sleep(1); */
		usleep(10000);
	/* start refresh timer in another thread */
	refreshRunning = true;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	if(pthread_create(&thread,&attr,refresh,NULL) != 0) {
		sim_error("cannot start refresh timer");
	}
}

static void vgaExit(void) {
	refreshRunning = false;
	run = false;
	/*sleep(1);*/
	usleep(10000);
	XSendEvent(vga.display,vga.win,False,0,(XEvent *)&vga.shutdown);
	XSync(vga.display,False);
	while(installed) /*sleep(1);*/
		usleep(10000);
}

static void vgaWrite(int x,int y,int r,int g,int b) {
	XPutPixel(vga.image, x, y, RGB2PIXEL(r, g, b));
}

/**************************************************************/
/**************************************************************/

static void updateCharacter(int x,int y,wyde c) {
	int xx,yy;
	int i,j;
	byte pixels;
	int r,g,b;

	xx = x * CELL_SIZE_X;
	yy = y * CELL_SIZE_Y;
	for(j = 0; j < CELL_SIZE_Y; j++) {
		pixels = font[(c & 0x00FF) * CELL_SIZE_Y + j];
		for(i = 0; i < CELL_SIZE_X; i++) {
			if((pixels & (1 << (CELL_SIZE_X - 1 - i))) != 0 && !((c & 0x8000) != 0 && blink)) {
				/* foreground */
				if(c & 0x0800) {
					/* intensify bit is on */
					r = (c & 0x0400) ? 255 : 73;
					g = (c & 0x0200) ? 255 : 73;
					b = (c & 0x0100) ? 255 : 73;
				}
				else {
					/* intensify bit is off */
					r = (c & 0x0400) ? 146 : 0;
					g = (c & 0x0200) ? 146 : 0;
					b = (c & 0x0100) ? 146 : 0;
				}
			}
			else {
				/* background */
				r = (c & 0x4000) ? 146 : 0;
				g = (c & 0x2000) ? 146 : 0;
				b = (c & 0x1000) ? 146 : 0;
			}
			vgaWrite(xx + i,yy + j,r,g,b);
		}
	}
}

static void blinkScreen(void) {
	static bool previousBlink = false;
	int x,y;
	wyde c;

	if(previousBlink == blink) {
		/* blink state didn't change, nothing to do */
		return;
	}
	previousBlink = blink;
	for(y = 0; y < TEXT_SIZE_Y; y++) {
		for(x = 0; x < TEXT_SIZE_X; x++) {
			/* update the character if it is blinking */
			c = text[y][x];
			if(c & 0x8000) {
				updateCharacter(x,y,c);
			}
		}
	}
}

static wyde readCharAndAttr(int x,int y) {
	return text[y][x];
}

static void writeCharAndAttr(int x,int y,wyde c) {
	text[y][x] = c;
	updateCharacter(x,y,c);
}

static void loadSplashScreen(void) {
	int x,y;
	const char *p;

	for(y = 0; y < TEXT_SIZE_Y; y++) {
		p = screen[y];
		for(x = 0; x < TEXT_SIZE_X; x++) {
			writeCharAndAttr(x,y,(wyde)0x0700 | (wyde)*p);
			p++;
		}
	}
}

/**************************************************************/

static octa displayRead(octa addr,bool sideEffects) {
	UNUSED(sideEffects);
	int x,y;

	if(!installed) {
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	x = (addr >> 3) & ((1 << 7) - 1);
	y = (addr >> 10) & ((1 << 5) - 1);
	if(x >= TEXT_SIZE_X || y >= TEXT_SIZE_Y) {
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	return readCharAndAttr(x,y);
}

static void displayWrite(octa addr,octa data) {
	int x,y;

	if(!installed) {
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	x = (addr >> 3) & ((1 << 7) - 1);
	y = (addr >> 10) & ((1 << 5) - 1);
	if(x >= TEXT_SIZE_X || y >= TEXT_SIZE_Y) {
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	writeCharAndAttr(x,y,data);
}

static void displayReset(void) {
	if(!installed) {
		return;
	}
	loadSplashScreen();
}

static void displayInit(void) {
	if(!installed) {
		vgaInit();
	}
	displayReset();
}

static void displayExit(void) {
	if(!installed) {
		return;
	}
	vgaExit();
}

/**************************************************************/
/**************************************************************/

static int keycodeCompare(const void *code1,const void *code2) {
	return ((Keycode *)code1)->xKeycode - ((Keycode *)code2)->xKeycode;
}

static void initKeycode(void) {
	qsort(kbdCodeTbl,sizeof(kbdCodeTbl) / sizeof(kbdCodeTbl[0]),sizeof(kbdCodeTbl[0]),
			keycodeCompare);
}

static Keycode *lookupKeycode(unsigned int xKeycode) {
	int lo,hi,tst;
	int res;

	lo = 0;
	hi = sizeof(kbdCodeTbl) / sizeof(kbdCodeTbl[0]) - 1;
	while(lo <= hi) {
		tst = (lo + hi) / 2;
		res = kbdCodeTbl[tst].xKeycode - xKeycode;
		if(res == 0) {
			return &kbdCodeTbl[tst];
		}
		if(res < 0) {
			lo = tst + 1;
		}
		else {
			hi = tst - 1;
		}
	}
	return NULL;
}

/**************************************************************/

static void kbdBufInit(void) {
	initKeycode();
	kbdBufWritePtr = 0;
	kbdBufReadPtr = 0;
}

static int kbdBufFree(void) {
	if(kbdBufReadPtr <= kbdBufWritePtr) {
		return KBD_BUF_MAX - (kbdBufWritePtr - kbdBufReadPtr) - 1;
	}
	else {
		return (kbdBufReadPtr - kbdBufWritePtr) - 1;
	}
}

static void keyPressed(unsigned int xKeycode) {
	Keycode *p;
	int i;

	if(debugKeycode) {
		mprintf("**** KEY PRESSED: 0x%08X ****\n",xKeycode);
	}
	p = lookupKeycode(xKeycode);
	if(p == NULL) {
		/* keycode not found */
		return;
	}
	if(kbdBufFree() < (MAX_MAKE + MAX_BREAK) * 4) {
		/* buffer full */
		return;
	}
	for(i = 0; i < p->pcNumMake; i++) {
		kbdBuf[kbdBufWritePtr] = p->pcKeyMake[i];
		kbdBufWritePtr = KBD_BUF_NEXT(kbdBufWritePtr);
	}
}

static void keyReleased(unsigned int xKeycode) {
	Keycode *p;
	int i;

	if(debugKeycode) {
		mprintf("**** KEY RELEASED: 0x%08X ****\n",xKeycode);
	}
	p = lookupKeycode(xKeycode);
	if(p == NULL) {
		/* keycode not found */
		return;
	}
	if(kbdBufFree() < MAX_BREAK * 4) {
		/* buffer full */
		return;
	}
	for(i = 0; i < p->pcNumBreak; i++) {
		kbdBuf[kbdBufWritePtr] = p->pcKeyBreak[i];
		kbdBufWritePtr = KBD_BUF_NEXT(kbdBufWritePtr);
	}
}

/**************************************************************/

static void kbdCallback(int dev) {
	if(debug) {
		mprintf("\n**** KEYBOARD CALLBACK ****\n");
	}
	timer_start(KEYBOARD_MSEC,&kbDev,kbdCallback,dev);
	if(kbdBufWritePtr == kbdBufReadPtr) {
		/* no character ready */
		return;
	}
	if(kbdCtrl & KEYBOARD_RDY) {
		/* last character not read yet */
		return;
	}
	/* any character typed */
	kbdData = kbdBuf[kbdBufReadPtr];
	kbdBufReadPtr = KBD_BUF_NEXT(kbdBufReadPtr);
	kbdCtrl |= KEYBOARD_RDY;
	if(kbdCtrl & KEYBOARD_IEN) {
		/* raise keyboard interrupt */
		cpu_setInterrupt(KEYBOARD_IRQ);
	}
}

static octa keyboardRead(octa addr,bool sideEffects) {
	octa data = 0;
	int reg = addr & DEV_REG_MASK;

	if(!installed) {
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	if(reg == KEYBOARD_CTRL) {
		data = kbdCtrl;
	}
	else if(reg == KEYBOARD_DATA) {
		if(sideEffects) {
			kbdCtrl &= ~KEYBOARD_RDY;
			if(kbdCtrl & KEYBOARD_IEN) {
				/* lower keyboard interrupt */
				cpu_resetInterrupt(KEYBOARD_IRQ);
			}
		}
		data = kbdData;
	}
	else {
		/* illegal register */
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	return data;
}

static void keyboardWrite(octa addr,octa data) {
	int reg = addr & DEV_REG_MASK;
	if(!installed) {
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	if(reg == KEYBOARD_CTRL) {
		if(data & KEYBOARD_IEN) {
			kbdCtrl |= KEYBOARD_IEN;
		}
		else {
			kbdCtrl &= ~KEYBOARD_IEN;
		}
		if(data & KEYBOARD_RDY) {
			kbdCtrl |= KEYBOARD_RDY;
		}
		else {
			kbdCtrl &= ~KEYBOARD_RDY;
		}
		if((kbdCtrl & KEYBOARD_IEN) != 0 && (kbdCtrl & KEYBOARD_RDY) != 0) {
			/* raise keyboard interrupt */
			cpu_setInterrupt(KEYBOARD_IRQ);
		}
		else {
			/* lower keyboard interrupt */
			cpu_resetInterrupt(KEYBOARD_IRQ);
		}
	}
	else if(reg == KEYBOARD_DATA) {
		/* this register is read-only */
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
	else {
		/* illegal register */
		ex_throw(EX_DYNAMIC_TRAP,TRAP_NONEX_MEMORY);
	}
}

static void keyboardReset(void) {
	if(!installed) {
		return;
	}
	kbdBufInit();
	kbdCtrl = 0;
	kbdData = 0;
	timer_start(KEYBOARD_MSEC,&kbDev,kbdCallback,0);
}

static void keyboardInit(void) {
	if(!installed) {
		vgaInit();
	}
	keyboardReset();
}

static void keyboardExit(void) {
	if(!installed) {
		return;
	}
	vgaExit();
}
