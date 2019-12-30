//========================================================================
//
// pdftotext.cc
//
// Copyright 1997-2013 Glyph & Cog, LLC
//
//========================================================================

// std c
#include <aconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

// std c++
#include <string>

// other libraries
#include <png.h>

// xpdf 4.02 includes
#include "gmem.h"
#include "gmempp.h"
#include "GlobalParams.h"
#include "Object.h"
#include "Gstring.h"
#include "Stream.h"
#include "Array.h"
#include "Dict.h"
#include "XRef.h"
#include "Catalog.h"
#include "Page.h"
#include "PDFDoc.h"
#include "SplashBitmap.h"
#include "Splash.h"
#include "SplashOutputDev.h"
#include "TextOutputDev.h"
#include "CharTypes.h"
#include "UnicodeMap.h"
#include "TextString.h"
#include "Error.h"
#include "config.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

using namespace std;

static int firstPage = 1;
static int lastPage = 0;
static bool physLayout = false;
static bool simpleLayout = false;
static bool tableLayout = false;
static bool linePrinter = false;
static bool rawOrder = false;
static double fixedPitch = 0;
static double fixedLineSpacing = 0;
static bool clipText = false;
static bool discardDiag = false;
static char textEncName[128] = "";
static char textEOL[16] = "";
static bool noPageBreaks = false;
static bool insertBOM = false;
static double marginLeft = 0;
static double marginRight = 0;
static double marginTop = 0;
static double marginBottom = 0;
static char ownerPassword[33] = "\001";
static char userPassword[33] = "\001";
static bool quiet = false;
static char cfgFileName[256] = "";
// PDF TO IMAGE
static double resolution = 150;
static GBool mono = gFalse;
static GBool gray = gFalse;
static GBool pngAlpha = gFalse;
static char enableFreeTypeStr[16] = "";
static char antialiasStr[16] = "";
static char vectorAntialiasStr[16] = "";

static void setupPNG(png_structp *png, png_infop *pngInfo, FILE *f,
		int bitDepth, int colorType, double res, SplashBitmap *bitmap);
static void writePNGData(png_structp png, SplashBitmap *bitmap);
static void finishPNG(png_structp *png, png_infop *pngInfo);

int main(int argc, char *argv[]) {
	// PDF TO TEXT
	PDFDoc *doc;
	std::string fileName;
	std::string textFileName;
	GString *ownerPasswordGString, *userPasswordGString;
	TextOutputControl textOutControl;
	TextOutputDev *textOut;
	UnicodeMap *uMap;
	int exitCode;
	exitCode = 99;
	bool flagExit = false;
	// PDF TO IMAGE
	SplashColor paperColor;
	SplashOutputDev *splashOut;
	int pg;
	png_structp png;
	png_infop pngInfo;
	FILE *f;
	GString *pngFile;
	char *pngRoot;

	fileName = "hello.pdf";

	if (strcmp(cfgFileName, "") == 0) {
		printf("No configuration file specified.\n");
	}
	// read config file
	globalParams = new GlobalParams(cfgFileName);
	//globalParams->setupBaseFonts("C:/Windows/Fonts");
	//globalParams->setPrintCommands(true);
	if (textEncName[0]) {
		globalParams->setTextEncoding(textEncName);
	}
	if (textEOL[0]) {
		if (!globalParams->setTextEOL(textEOL)) {
			printf("Bad 'eol' value");
		}
	}
	if (enableFreeTypeStr[0]) {
		if (!globalParams->setEnableFreeType(enableFreeTypeStr)) {
			printf("Bad 'freetype' value");
		}
	}
	if (antialiasStr[0]) {
		if (!globalParams->setAntialias(antialiasStr)) {
			printf("Bad 'antialias' value");
		}
	}
	if (vectorAntialiasStr[0]) {
		if (!globalParams->setVectorAntialias(vectorAntialiasStr)) {
			printf("Bad 'vectorAntialias' value");
		}
	}
	if (noPageBreaks) {
		globalParams->setTextPageBreaks(false);
	}
	if (quiet) {
		globalParams->setErrQuiet(quiet);
	}

	// get mapping to output encoding
	if (!(uMap = globalParams->getTextEncoding())) {
		flagExit = true;
		printf("Couldn't get text encoding\n");
	}

	if (!flagExit) {
		// open PDF file
		if (ownerPassword[0] != '\001') {
			ownerPasswordGString = new GString(ownerPassword);
		} else {
			ownerPasswordGString = NULL;
		}
		if (userPassword[0] != '\001') {
			userPasswordGString = new GString(userPassword);
		} else {
			userPasswordGString = NULL;
		}
		GString* fileNameGString = new GString(fileName.c_str());
		printf("Loading PDF\n");
		doc = new PDFDoc(fileNameGString, ownerPasswordGString,
				userPasswordGString);

		if (!doc->isOk()) {
			exitCode = 1;
			flagExit = true;
		}

		// check for copy permission
		if (!(doc->okToCopy())) {
			printf("Copying of text from this document is not allowed.\n");
			exitCode = 3;
			flagExit = true;
		}
	}

	if (!flagExit) {
		// construct text file name
		if (fileName.compare(fileName.size() - 4, 4, ".pdf") == 0
				|| fileName.compare(fileName.size() - 4, 4, ".PDF") == 0) {
			textFileName = fileName.substr(0, fileName.size() - 4);
		} else {
			textFileName = fileName;
		}
		pngRoot = const_cast<char*>(textFileName.c_str());
		textFileName.append(".txt");

		// get page range
		if (firstPage < 1) {
			firstPage = 1;
		}
		if (lastPage < 1 || lastPage > doc->getNumPages()) {
			lastPage = doc->getNumPages();
		}

		rawOrder = true;
		// write text file

		if (tableLayout) {
			textOutControl.mode = textOutTableLayout;
			textOutControl.fixedPitch = fixedPitch;
		} else if (physLayout) {
			textOutControl.mode = textOutPhysLayout;
			textOutControl.fixedPitch = fixedPitch;
		} else if (simpleLayout) {
			textOutControl.mode = textOutSimpleLayout;
		} else if (linePrinter) {
			textOutControl.mode = textOutLinePrinter;
			textOutControl.fixedPitch = fixedPitch;
			textOutControl.fixedLineSpacing = fixedLineSpacing;
		} else if (rawOrder) {
			textOutControl.mode = textOutRawOrder;
		} else {
			textOutControl.mode = textOutReadingOrder;
		}
		textOutControl.clipText = clipText;
		textOutControl.discardDiagonalText = discardDiag;
		textOutControl.insertBOM = insertBOM;
		textOutControl.marginLeft = marginLeft;
		textOutControl.marginRight = marginRight;
		textOutControl.marginTop = marginTop;
		textOutControl.marginBottom = marginBottom;
		textOut = new TextOutputDev(const_cast<char*>(textFileName.c_str()),
				&textOutControl, false);
		if (textOut->isOk()) {
			doc->displayPages(textOut, firstPage, lastPage, 72, 72, 0, false,
					true, false);
		} else {
			delete textOut;
			exitCode = 2;
		}
		delete textOut;

		exitCode = 0;

		// PDF TO IMAGE
		// write PNG files

		if (mono) {
			paperColor[0] = 0xff;
			splashOut = new SplashOutputDev(splashModeMono1, 1, false,
					paperColor);
		} else if (gray) {
			paperColor[0] = 0xff;
			splashOut = new SplashOutputDev(splashModeMono8, 1, false,
					paperColor);
		} else {
			paperColor[0] = paperColor[1] = paperColor[2] = 0xff;
			splashOut = new SplashOutputDev(splashModeRGB8, 1, false,
					paperColor);
		}
		if (pngAlpha) {
			splashOut->setNoComposite(gTrue);
		}

		splashOut->startDoc(doc->getXRef());
		for (pg = firstPage; pg <= lastPage; ++pg) {
			doc->displayPage(splashOut, pg, resolution, resolution, 0, false,
					true, false);
			if (mono) {
				if (!strcmp(pngRoot, "-")) {
					f = stdout;
#ifdef _WIN32
					_setmode(_fileno(f), _O_BINARY);
#endif
				} else {
					pngFile = GString::format("{0:s}-{1:06d}.png", pngRoot, pg);
					if (!(f = fopen(pngFile->getCString(), "wb"))) {
						exit(2);
					}
					delete pngFile;
				}
				setupPNG(&png, &pngInfo, f, 1, PNG_COLOR_TYPE_GRAY, resolution,
						splashOut->getBitmap());
				writePNGData(png, splashOut->getBitmap());
				finishPNG(&png, &pngInfo);
				fclose(f);
			} else if (gray) {
				if (!strcmp(pngRoot, "-")) {
					f = stdout;
#ifdef _WIN32
					_setmode(_fileno(f), _O_BINARY);
#endif
				} else {
					pngFile = GString::format("{0:s}-{1:06d}.png", pngRoot, pg);
					if (!(f = fopen(pngFile->getCString(), "wb"))) {
						exit(2);
					}
					delete pngFile;
				}
				setupPNG(&png, &pngInfo, f, 8, pngAlpha ?
				PNG_COLOR_TYPE_GRAY_ALPHA :
															PNG_COLOR_TYPE_GRAY,
						resolution, splashOut->getBitmap());
				writePNGData(png, splashOut->getBitmap());
				finishPNG(&png, &pngInfo);
				fclose(f);
			} else { // RGB
				if (!strcmp(pngRoot, "-")) {
					f = stdout;
#ifdef _WIN32
					_setmode(_fileno(f), _O_BINARY);
#endif
				} else {
					pngFile = GString::format("{0:s}-{1:06d}.png", pngRoot, pg);
					if (!(f = fopen(pngFile->getCString(), "wb"))) {
						exit(2);
					}
					delete pngFile;
				}
				setupPNG(&png, &pngInfo, f, 8, pngAlpha ?
				PNG_COLOR_TYPE_RGB_ALPHA :
															PNG_COLOR_TYPE_RGB,
						resolution, splashOut->getBitmap());
				writePNGData(png, splashOut->getBitmap());
				finishPNG(&png, &pngInfo);
				fclose(f);
			}
		}
		delete splashOut;

		exitCode = 0;

		// clean up
		delete doc;
		uMap->decRefCnt();
	}
	delete globalParams;

	system("pause");
	return exitCode;
}

static void setupPNG(png_structp *png, png_infop *pngInfo, FILE *f,
		int bitDepth, int colorType, double res, SplashBitmap *bitmap) {
	png_color_16 background;
	int pixelsPerMeter;

	if (!(*png = png_create_write_struct(PNG_LIBPNG_VER_STRING,
	NULL, NULL, NULL)) || !(*pngInfo = png_create_info_struct(*png))) {
		exit(2);
	}
	if (setjmp(png_jmpbuf(*png))) {
		exit(2);
	}
	png_init_io(*png, f);
	png_set_IHDR(*png, *pngInfo, bitmap->getWidth(), bitmap->getHeight(),
			bitDepth, colorType, PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	if (colorType == PNG_COLOR_TYPE_GRAY_ALPHA
			|| colorType == PNG_COLOR_TYPE_RGB_ALPHA) {
		background.index = 0;
		background.red = 0xff;
		background.green = 0xff;
		background.blue = 0xff;
		background.gray = 0xff;
		png_set_bKGD(*png, *pngInfo, &background);
	}
	pixelsPerMeter = (int) (res * (1000 / 25.4) + 0.5);
	png_set_pHYs(*png, *pngInfo, pixelsPerMeter, pixelsPerMeter,
	PNG_RESOLUTION_METER);
	png_write_info(*png, *pngInfo);
}

static void writePNGData(png_structp png, SplashBitmap *bitmap) {
	Guchar *p, *alpha, *rowBuf, *rowBufPtr;
	int y, x;

	if (setjmp(png_jmpbuf(png))) {
		exit(2);
	}
	p = bitmap->getDataPtr();
	if (pngAlpha) {
		alpha = bitmap->getAlphaPtr();
		if (bitmap->getMode() == splashModeMono8) {
			rowBuf = (Guchar *) gmallocn(bitmap->getWidth(), 2);
			for (y = 0; y < bitmap->getHeight(); ++y) {
				rowBufPtr = rowBuf;
				for (x = 0; x < bitmap->getWidth(); ++x) {
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *alpha++;
				}
				png_write_row(png, (png_bytep) rowBuf);
			}
			gfree(rowBuf);
		} else { // splashModeRGB8
			rowBuf = (Guchar *) gmallocn(bitmap->getWidth(), 4);
			for (y = 0; y < bitmap->getHeight(); ++y) {
				rowBufPtr = rowBuf;
				for (x = 0; x < bitmap->getWidth(); ++x) {
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *p++;
					*rowBufPtr++ = *alpha++;
				}
				png_write_row(png, (png_bytep) rowBuf);
			}
			gfree(rowBuf);
		}
	} else {
		for (y = 0; y < bitmap->getHeight(); ++y) {
			png_write_row(png, (png_bytep) p);
			p += bitmap->getRowSize();
		}
	}
}

static void finishPNG(png_structp *png, png_infop *pngInfo) {
	if (setjmp(png_jmpbuf(*png))) {
		exit(2);
	}
	png_write_end(*png, *pngInfo);
	png_destroy_write_struct(png, pngInfo);
}
