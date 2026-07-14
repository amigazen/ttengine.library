/*
 * TTEngineTest - command-line unit tests for ttengine.library
 *
 * Exercises public LVOs as far as is practical without an interactive GUI.
 * TT_Request is skipped unless REQUEST=1 is passed on the command line.
 *
 * Font selection (first match wins):
 *   FONT=<path>     open by TT_FontFile
 *   FAMILY=<name>   open via TT_FamilyTable
 *   (default)       first entry from TT_ObtainFamilyList, else "default"
 *
 * Optional:
 *   SIZE=<pixels>   font size (default 18)
 *   REQUEST=1       run interactive font requester smoke test
 */

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/gfx.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <utility/tagitem.h>
#include <libraries/ttengine.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/ttengine.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BM_WIDTH   320
#define BM_HEIGHT  64
#define BM_DEPTH   1

#define SAMPLE_ASCII   "Hello TTEngine"
#define SAMPLE_DIGITS  "0123456789"
#define SAMPLE_MIXED   "AaBbYyGgQq"

struct Library *TTEngineBase;
struct GfxBase *GfxBase;
struct Library *UtilityBase;

static LONG test_total;
static LONG test_passed;
static LONG test_failed;
static BOOL suite_fail;

static char g_fontfile[256];
static char g_family[128];
static LONG g_fontsize;
static BOOL g_do_request;

static APTR g_font;
static struct BitMap *g_bm;
static struct RastPort g_rp;
static BOOL g_rp_ready;

static const char *sample_ascii = SAMPLE_ASCII;
static const char *sample_digits = SAMPLE_DIGITS;
static const char *sample_mixed = SAMPLE_MIXED;

/* ----------------------------------------------------------------------- */

static void print_api_map(void)
{
	printf("Public API under test (ttengine.library V7):\n");
	printf("  TT_OpenFontA / TT_CloseFont\n");
	printf("  TT_SetFont / TT_DoneRastPort\n");
	printf("  TT_SetAttrsA / TT_GetAttrsA\n");
	printf("  TT_Text / TT_TextLength / TT_TextExtent / TT_TextFit\n");
	printf("  TT_GetPixmapA / TT_FreePixmap\n");
	printf("  TT_AllocRequest / TT_FreeRequest");
	printf("  [TT_RequestA if REQUEST=1]\n");
	printf("  TT_ObtainFamilyListA / TT_FreeFamilyList\n");
	printf("\n");
}

static void suite_begin(const char *title, const char *purpose, const char *apis)
{
	test_total++;
	suite_fail = FALSE;
	printf("TEST %ld: %s\n", test_total, title);
	printf("  Purpose: %s\n", purpose);
	printf("  APIs:    %s\n", apis);
}

static void suite_end(void)
{
	if (suite_fail) {
		test_failed++;
		printf("  RESULT: FAIL\n\n");
	} else {
		test_passed++;
		printf("  RESULT: PASS\n\n");
	}
}

static BOOL expect_true(const char *check, BOOL actual)
{
	printf("  - %s\n", check);
	printf("      Expect: TRUE\n");
	printf("      Actual: %s\n", actual ? "TRUE" : "FALSE");
	if (!actual) {
		suite_fail = TRUE;
		return FALSE;
	}
	return TRUE;
}

static BOOL expect_ptr_nonnull(const char *check, const void *actual)
{
	printf("  - %s\n", check);
	printf("      Expect: non-NULL\n");
	printf("      Actual: %s\n", actual ? "non-NULL" : "NULL");
	if (actual == NULL) {
		suite_fail = TRUE;
		return FALSE;
	}
	return TRUE;
}

static BOOL expect_ptr_null(const char *check, const void *actual)
{
	printf("  - %s\n", check);
	printf("      Expect: NULL\n");
	printf("      Actual: %s\n", actual ? "non-NULL" : "NULL");
	if (actual != NULL) {
		suite_fail = TRUE;
		return FALSE;
	}
	return TRUE;
}

static BOOL expect_ulong_eq(const char *check, ULONG expected, ULONG actual)
{
	printf("  - %s\n", check);
	printf("      Expect: %lu\n", expected);
	printf("      Actual: %lu\n", actual);
	if (actual != expected) {
		suite_fail = TRUE;
		return FALSE;
	}
	return TRUE;
}

static BOOL expect_ulong_ge(const char *check, ULONG minimum, ULONG actual)
{
	printf("  - %s\n", check);
	printf("      Expect: >= %lu\n", minimum);
	printf("      Actual: %lu\n", actual);
	if (actual < minimum) {
		suite_fail = TRUE;
		return FALSE;
	}
	return TRUE;
}

static void print_summary(void)
{
	printf("=== Summary ===\n");
	printf("  Total:  %ld\n", test_total);
	printf("  Passed: %ld\n", test_passed);
	printf("  Failed: %ld\n", test_failed);
	if (test_failed == 0) {
		printf("ALL TESTS PASSED\n");
	} else {
		printf("SOME TESTS FAILED\n");
	}
}

/* ----------------------------------------------------------------------- */

static void parse_args(int argc, char **argv)
{
	int i;
	char *eq;
	char *key;
	char *val;

	g_fontfile[0] = '\0';
	g_family[0] = '\0';
	g_fontsize = 18;
	g_do_request = FALSE;

	for (i = 1; i < argc; i++) {
		eq = strchr(argv[i], '=');
		if (eq == NULL) {
			continue;
		}
		*eq = '\0';
		key = argv[i];
		val = eq + 1;

		if (stricmp(key, "FONT") == 0) {
			strncpy(g_fontfile, val, sizeof(g_fontfile) - 1);
			g_fontfile[sizeof(g_fontfile) - 1] = '\0';
		} else if (stricmp(key, "FAMILY") == 0) {
			strncpy(g_family, val, sizeof(g_family) - 1);
			g_family[sizeof(g_family) - 1] = '\0';
		} else if (stricmp(key, "SIZE") == 0) {
			g_fontsize = strtol(val, NULL, 10);
			if (g_fontsize <= 0) {
				g_fontsize = 18;
			}
		} else if (stricmp(key, "REQUEST") == 0) {
			if (stricmp(val, "1") == 0 || stricmp(val, "YES") == 0
			    || stricmp(val, "TRUE") == 0) {
				g_do_request = TRUE;
			}
		}
		*eq = '=';
	}
}

static BOOL open_libraries(void)
{
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
	if (GfxBase == NULL) {
		printf("ERROR: cannot open graphics.library\n");
		return FALSE;
	}

	UtilityBase = OpenLibrary("utility.library", 37);
	if (UtilityBase == NULL) {
		printf("ERROR: cannot open utility.library\n");
		return FALSE;
	}

	/* Prefer an in-tree build next to PROGDIR, then installed LIBS: name. */
	TTEngineBase = OpenLibrary("PROGDIR:../ttengine.library", TTENGINEVERSION);
	if (TTEngineBase == NULL) {
		TTEngineBase = OpenLibrary(TTENGINENAME, TTENGINEVERSION);
	}
	if (TTEngineBase == NULL) {
		printf("ERROR: cannot open %s V%ld+\n",
			TTENGINENAME, (LONG)TTENGINEVERSION);
		printf("  Tried PROGDIR:../ttengine.library and %s\n",
			TTENGINENAME);
		return FALSE;
	}

	printf("Opened %s V%ld\n\n", TTEngineBase->lib_Node.ln_Name,
		(LONG)TTEngineBase->lib_Version);
	return TRUE;
}

static void close_libraries(void)
{
	if (TTEngineBase != NULL) {
		CloseLibrary(TTEngineBase);
		TTEngineBase = NULL;
	}
	if (UtilityBase != NULL) {
		CloseLibrary(UtilityBase);
		UtilityBase = NULL;
	}
	if (GfxBase != NULL) {
		CloseLibrary((struct Library *)GfxBase);
		GfxBase = NULL;
	}
}

static BOOL create_rastport(void)
{
	g_bm = AllocBitMap(BM_WIDTH, BM_HEIGHT, BM_DEPTH, BMF_CLEAR, NULL);
	if (g_bm == NULL) {
		printf("ERROR: AllocBitMap failed\n");
		return FALSE;
	}

	InitRastPort(&g_rp);
	g_rp.BitMap = g_bm;
	SetAPen(&g_rp, 1);
	SetBPen(&g_rp, 0);
	SetDrMd(&g_rp, JAM2);
	Move(&g_rp, 8, (WORD)(g_fontsize + 4));
	g_rp_ready = TRUE;
	return TRUE;
}

static void destroy_rastport(void)
{
	if (g_rp_ready) {
		TT_DoneRastPort(&g_rp);
		g_rp_ready = FALSE;
	}
	if (g_bm != NULL) {
		FreeBitMap(g_bm);
		g_bm = NULL;
	}
}

static APTR open_test_font(void)
{
	APTR font;
	STRPTR *families;
	STRPTR family_table[3];
	struct TagItem tags[8];
	WORD ti;

	font = NULL;
	ti = 0;

	if (g_fontfile[0] != '\0') {
		tags[ti].ti_Tag = TT_FontFile;
		tags[ti].ti_Data = (ULONG)g_fontfile;
		ti++;
		tags[ti].ti_Tag = TT_FontSize;
		tags[ti].ti_Data = (ULONG)g_fontsize;
		ti++;
		tags[ti].ti_Tag = TAG_DONE;
		tags[ti].ti_Data = 0;
		font = TT_OpenFontA(tags);
		printf("Open by FONT=%s SIZE=%ld -> %s\n",
			g_fontfile, g_fontsize, font ? "ok" : "FAIL");
		return font;
	}

	if (g_family[0] != '\0') {
		family_table[0] = g_family;
		family_table[1] = "default";
		family_table[2] = NULL;
	} else {
		families = TT_ObtainFamilyListA(NULL);
		if (families != NULL && families[0] != NULL) {
			strncpy(g_family, families[0], sizeof(g_family) - 1);
			g_family[sizeof(g_family) - 1] = '\0';
			TT_FreeFamilyList(families);
		} else {
			if (families != NULL) {
				TT_FreeFamilyList(families);
			}
			strcpy(g_family, "default");
		}
		family_table[0] = g_family;
		family_table[1] = "default";
		family_table[2] = NULL;
	}

	tags[ti].ti_Tag = TT_FamilyTable;
	tags[ti].ti_Data = (ULONG)family_table;
	ti++;
	tags[ti].ti_Tag = TT_FontSize;
	tags[ti].ti_Data = (ULONG)g_fontsize;
	ti++;
	tags[ti].ti_Tag = TT_FontStyle;
	tags[ti].ti_Data = TT_FontStyle_Regular;
	ti++;
	tags[ti].ti_Tag = TT_FontWeight;
	tags[ti].ti_Data = TT_FontWeight_Normal;
	ti++;
	tags[ti].ti_Tag = TAG_DONE;
	tags[ti].ti_Data = 0;

	font = TT_OpenFontA(tags);
	printf("Open by FAMILY=%s SIZE=%ld -> %s\n",
		g_family, g_fontsize, font ? "ok" : "FAIL");
	return font;
}

/* ----------------------------------------------------------------------- */

static void test_null_safety(void)
{
	suite_begin("NULL / no-op safety",
		"Documented NULL-tolerant cleanup paths must not crash",
		"TT_CloseFont, TT_DoneRastPort, TT_FreePixmap, "
		"TT_FreeFamilyList, TT_FreeRequest, TT_RequestA");

	TT_CloseFont(NULL);
	expect_true("TT_CloseFont(NULL) returned", TRUE);

	TT_DoneRastPort(NULL);
	expect_true("TT_DoneRastPort(NULL) returned", TRUE);

	TT_FreePixmap(NULL);
	expect_true("TT_FreePixmap(NULL) returned", TRUE);

	TT_FreeFamilyList(NULL);
	expect_true("TT_FreeFamilyList(NULL) returned", TRUE);

	TT_FreeRequest(NULL);
	expect_true("TT_FreeRequest(NULL) returned", TRUE);

	expect_ptr_null("TT_RequestA(NULL, NULL)",
		TT_RequestA(NULL, NULL));

	suite_end();
}

static void test_open_failures(void)
{
	APTR font;
	struct TagItem tags[4];

	suite_begin("TT_OpenFontA failure paths",
		"Zero size and missing file must return NULL",
		"TT_OpenFontA, TT_CloseFont");

	tags[0].ti_Tag = TT_FontFile;
	tags[0].ti_Data = (ULONG)"T:__ttengine_missing__.ttf";
	tags[1].ti_Tag = TT_FontSize;
	tags[1].ti_Data = 18;
	tags[2].ti_Tag = TAG_DONE;
	tags[2].ti_Data = 0;
	font = TT_OpenFontA(tags);
	expect_ptr_null("missing FONT file", font);
	TT_CloseFont(font);

	tags[0].ti_Tag = TT_FontSize;
	tags[0].ti_Data = 0;
	tags[1].ti_Tag = TAG_DONE;
	tags[1].ti_Data = 0;
	font = TT_OpenFontA(tags);
	expect_ptr_null("FontSize 0", font);
	TT_CloseFont(font);

	suite_end();
}

static void test_family_list(void)
{
	STRPTR *list;
	LONG count;
	LONG i;

	suite_begin("TT_ObtainFamilyListA",
		"Family list is NULL-terminated and freeable",
		"TT_ObtainFamilyListA, TT_FreeFamilyList");

	list = TT_ObtainFamilyListA(NULL);
	if (list == NULL) {
		printf("  - obtain list\n");
		printf("      Note: NULL (no ttengine.database / empty)\n");
		printf("      Treated as acceptable on a bare install\n");
		expect_true("NULL list allowed", TRUE);
	} else {
		count = 0;
		for (i = 0; list[i] != NULL; i++) {
			count++;
			if (count <= 5) {
				printf("  - family[%ld] = \"%s\"\n", i, list[i]);
			}
		}
		expect_ulong_ge("family count", 0, (ULONG)count);
		TT_FreeFamilyList(list);
		expect_true("TT_FreeFamilyList completed", TRUE);
	}

	suite_end();
}

static void test_open_setfont(void)
{
	BOOL ok;

	suite_begin("Open / SetFont / Close",
		"Open a face, attach to RastPort, then close cleanly",
		"TT_OpenFontA, TT_SetFont, TT_CloseFont, TT_DoneRastPort");

	expect_ptr_nonnull("test font opened", g_font);
	if (g_font == NULL) {
		suite_end();
		return;
	}

	ok = TT_SetFont(&g_rp, g_font);
	expect_true("TT_SetFont", ok);

	TT_DoneRastPort(&g_rp);
	expect_true("TT_DoneRastPort after SetFont", TRUE);

	/* Re-attach for subsequent suites that share g_font / g_rp. */
	ok = TT_SetFont(&g_rp, g_font);
	expect_true("TT_SetFont re-attach", ok);

	suite_end();
}

static void test_attrs(void)
{
	ULONG count;
	ULONG antialias;
	ULONG transparency;
	ULONG gamma;
	ULONG ascender;
	ULONG descender;
	ULONG maxtop;
	ULONG maxbottom;
	ULONG design_h;
	ULONG real_asc;
	ULONG real_desc;
	ULONG accented;
	ULONG font_h;
	ULONG font_w;
	ULONG fixed;
	STRPTR font_name;
	STRPTR family_name;
	STRPTR subfamily;
	struct TagItem get_tags[20];
	struct TagItem set_tags[8];
	WORD ti;

	/*
	 * GetAttrs only returns attrs implemented in lib.c (metrics, names,
	 * Antialias, Transparency, Gamma). TT_Encoding / TT_FontSize /
	 * TT_FontStyle / TT_FontWeight / TT_FontFile are not gettable despite
	 * the 'G' marks in libraries/ttengine.h.
	 */
	suite_begin("TT_SetAttrsA / TT_GetAttrsA",
		"Set render attrs and read back implemented GetAttrs fields",
		"TT_SetAttrsA, TT_GetAttrsA");

	if (g_font == NULL) {
		expect_true("skipped (no font)", FALSE);
		suite_end();
		return;
	}

	set_tags[0].ti_Tag = TT_Antialias;
	set_tags[0].ti_Data = TT_Antialias_Off;
	set_tags[1].ti_Tag = TT_Encoding;
	set_tags[1].ti_Data = TT_Encoding_ISO8859_1;
	set_tags[2].ti_Tag = TT_Transparency;
	set_tags[2].ti_Data = 0;
	set_tags[3].ti_Tag = TT_SoftStyle;
	set_tags[3].ti_Data = TT_SoftStyle_None;
	set_tags[4].ti_Tag = TT_Foreground;
	set_tags[4].ti_Data = TT_Foreground_UseRastPort;
	set_tags[5].ti_Tag = TT_Background;
	set_tags[5].ti_Data = TT_Background_UseRastPort;
	set_tags[6].ti_Tag = TT_Gamma;
	set_tags[6].ti_Data = 2500;
	set_tags[7].ti_Tag = TAG_DONE;
	set_tags[7].ti_Data = 0;

	count = TT_SetAttrsA(&g_rp, set_tags);
	expect_ulong_ge("SetAttrs recognized tags", 1, count);

	antialias = 0xFFFFFFFFul;
	transparency = 0xFFFFFFFFul;
	gamma = 0;
	ascender = 0;
	descender = 0;
	maxtop = 0;
	maxbottom = 0;
	design_h = 0;
	real_asc = 0;
	real_desc = 0;
	accented = 0;
	font_h = 0;
	font_w = 0;
	fixed = 0xFFFFFFFFul;
	font_name = NULL;
	family_name = NULL;
	subfamily = NULL;

	ti = 0;
	get_tags[ti].ti_Tag = TT_Antialias;
	get_tags[ti].ti_Data = (ULONG)&antialias;
	ti++;
	get_tags[ti].ti_Tag = TT_Transparency;
	get_tags[ti].ti_Data = (ULONG)&transparency;
	ti++;
	get_tags[ti].ti_Tag = TT_Gamma;
	get_tags[ti].ti_Data = (ULONG)&gamma;
	ti++;
	get_tags[ti].ti_Tag = TT_FontAscender;
	get_tags[ti].ti_Data = (ULONG)&ascender;
	ti++;
	get_tags[ti].ti_Tag = TT_FontDescender;
	get_tags[ti].ti_Data = (ULONG)&descender;
	ti++;
	get_tags[ti].ti_Tag = TT_FontMaxTop;
	get_tags[ti].ti_Data = (ULONG)&maxtop;
	ti++;
	get_tags[ti].ti_Tag = TT_FontMaxBottom;
	get_tags[ti].ti_Data = (ULONG)&maxbottom;
	ti++;
	get_tags[ti].ti_Tag = TT_FontDesignHeight;
	get_tags[ti].ti_Data = (ULONG)&design_h;
	ti++;
	get_tags[ti].ti_Tag = TT_FontRealAscender;
	get_tags[ti].ti_Data = (ULONG)&real_asc;
	ti++;
	get_tags[ti].ti_Tag = TT_FontRealDescender;
	get_tags[ti].ti_Data = (ULONG)&real_desc;
	ti++;
	get_tags[ti].ti_Tag = TT_FontAccentedAscender;
	get_tags[ti].ti_Data = (ULONG)&accented;
	ti++;
	get_tags[ti].ti_Tag = TT_FontHeight;
	get_tags[ti].ti_Data = (ULONG)&font_h;
	ti++;
	get_tags[ti].ti_Tag = TT_FontWidth;
	get_tags[ti].ti_Data = (ULONG)&font_w;
	ti++;
	get_tags[ti].ti_Tag = TT_FontFixedWidth;
	get_tags[ti].ti_Data = (ULONG)&fixed;
	ti++;
	get_tags[ti].ti_Tag = TT_FontName;
	get_tags[ti].ti_Data = (ULONG)&font_name;
	ti++;
	get_tags[ti].ti_Tag = TT_FamilyName;
	get_tags[ti].ti_Data = (ULONG)&family_name;
	ti++;
	get_tags[ti].ti_Tag = TT_SubfamilyName;
	get_tags[ti].ti_Data = (ULONG)&subfamily;
	ti++;
	get_tags[ti].ti_Tag = TAG_DONE;
	get_tags[ti].ti_Data = 0;

	count = TT_GetAttrsA(&g_rp, get_tags);
	expect_ulong_ge("GetAttrs recognized tags", 10, count);

	expect_ulong_eq("Antialias Off", TT_Antialias_Off, antialias);
	expect_ulong_eq("Transparency 0", 0, transparency);
	expect_ulong_eq("Gamma 2500", 2500, gamma);
	expect_ulong_ge("FontAscender", 1, ascender);
	expect_ulong_ge("FontDesignHeight", 1, design_h);
	expect_ulong_ge("FontHeight", 1, font_h);

	printf("  - FontName=\"%s\" Family=\"%s\" Subfamily=\"%s\"\n",
		font_name ? font_name : "(null)",
		family_name ? family_name : "(null)",
		subfamily ? subfamily : "(null)");
	printf("  - metrics: asc=%lu desc=%ld maxTop=%lu maxBot=%ld "
		"realAsc=%lu realDesc=%ld accented=%lu width=%lu fixed=%lu\n",
		ascender, (LONG)descender, maxtop, (LONG)maxbottom,
		real_asc, (LONG)real_desc, accented, font_w, fixed);

	expect_ptr_nonnull("FontName", font_name);
	expect_ptr_nonnull("FamilyName", family_name);

	set_tags[0].ti_Tag = TT_SoftStyle;
	set_tags[0].ti_Data = TT_SoftStyle_Underlined | TT_SoftStyle_Overstriked;
	set_tags[1].ti_Tag = TT_Antialias;
	set_tags[1].ti_Data = TT_Antialias_Auto;
	set_tags[2].ti_Tag = TT_DiskFontMetrics;
	set_tags[2].ti_Data = FALSE;
	set_tags[3].ti_Tag = TT_ForceFixedWidth;
	set_tags[3].ti_Data = 0;
	set_tags[4].ti_Tag = TAG_DONE;
	set_tags[4].ti_Data = 0;
	count = TT_SetAttrsA(&g_rp, set_tags);
	expect_ulong_eq("SetAttrs softstyle/antialias/metrics", 4, count);

	suite_end();
}

static void test_text_metrics(void)
{
	ULONG len_hello;
	ULONG len_digits;
	ULONG len_empty;
	struct TextExtent te;
	struct TextExtent constrain;
	ULONG fit;
	ULONG count;

	suite_begin("Text length / extent / fit",
		"Metric helpers agree on non-empty strings",
		"TT_TextLength, TT_TextExtent, TT_TextFit");

	if (g_font == NULL) {
		expect_true("skipped (no font)", FALSE);
		suite_end();
		return;
	}

	len_hello = TT_TextLength(&g_rp, (APTR)sample_ascii,
		(ULONG)strlen(sample_ascii));
	len_digits = TT_TextLength(&g_rp, (APTR)sample_digits,
		(ULONG)strlen(sample_digits));
	len_empty = TT_TextLength(&g_rp, (APTR)"", 0);

	expect_ulong_ge("TextLength(Hello)", 1, len_hello);
	expect_ulong_ge("TextLength(digits)", 1, len_digits);
	expect_ulong_eq("TextLength(empty)", 0, len_empty);

	memset(&te, 0, sizeof(te));
	TT_TextExtent(&g_rp, (APTR)sample_mixed, (WORD)strlen(sample_mixed), &te);
	printf("  - TextExtent te_Width=%ld te_Height=%ld "
		"te_Extent=(%ld,%ld)-(%ld,%ld)\n",
		(LONG)te.te_Width, (LONG)te.te_Height,
		(LONG)te.te_Extent.MinX, (LONG)te.te_Extent.MinY,
		(LONG)te.te_Extent.MaxX, (LONG)te.te_Extent.MaxY);
	expect_ulong_ge("te_Width", 1, (ULONG)te.te_Width);
	expect_ulong_ge("te_Height", 1, (ULONG)te.te_Height);

	/* Constrain width so TextFit must return fewer than full string chars. */
	memset(&constrain, 0, sizeof(constrain));
	constrain.te_Width = (WORD)(len_hello / 2);
	if (constrain.te_Width < 1) {
		constrain.te_Width = 1;
	}
	constrain.te_Height = (WORD)(g_fontsize * 2);
	constrain.te_Extent.MinX = 0;
	constrain.te_Extent.MinY = -(WORD)g_fontsize;
	constrain.te_Extent.MaxX = constrain.te_Width;
	constrain.te_Extent.MaxY = (WORD)g_fontsize;

	count = (ULONG)strlen(sample_ascii);
	fit = TT_TextFit(&g_rp, (APTR)sample_ascii, (UWORD)count,
		&te, &constrain, 1, constrain.te_Width, constrain.te_Height);
	printf("  - TextFit chars=%lu of %lu (constrain width=%ld)\n",
		fit, count, (LONG)constrain.te_Width);
	expect_ulong_ge("TextFit >= 0", 0, fit);
	expect_true("TextFit <= strlen", fit <= count);

	suite_end();
}

static void test_text_render(void)
{
	suite_begin("TT_Text render",
		"Render sample strings into offscreen RastPort",
		"TT_Text, TT_SetAttrsA");

	if (g_font == NULL) {
		expect_true("skipped (no font)", FALSE);
		suite_end();
		return;
	}

	Move(&g_rp, 8, (WORD)(g_fontsize + 4));
	TT_Text(&g_rp, (APTR)sample_ascii, (ULONG)strlen(sample_ascii));
	expect_true("TT_Text(ASCII)", TRUE);

	Move(&g_rp, 8, (WORD)(g_fontsize * 2 + 8));
	TT_Text(&g_rp, (APTR)sample_digits, (ULONG)strlen(sample_digits));
	expect_true("TT_Text(digits)", TRUE);

	suite_end();
}

static void test_pixmap(void)
{
	struct TT_Pixmap *pm;
	struct TagItem tags[4];
	ULONG i;
	ULONG nonzero;

	suite_begin("TT_GetPixmapA / TT_FreePixmap",
		"Grayscale pixmap has sane dimensions and some ink",
		"TT_GetPixmapA, TT_FreePixmap");

	if (g_font == NULL) {
		expect_true("skipped (no font)", FALSE);
		suite_end();
		return;
	}

	tags[0].ti_Tag = TT_Antialias;
	tags[0].ti_Data = TT_Antialias_On;
	tags[1].ti_Tag = TT_SoftStyle;
	tags[1].ti_Data = TT_SoftStyle_None;
	tags[2].ti_Tag = TAG_DONE;
	tags[2].ti_Data = 0;

	pm = TT_GetPixmapA(g_font, (APTR)sample_ascii,
		(ULONG)strlen(sample_ascii), tags);
	expect_ptr_nonnull("pixmap", pm);
	if (pm == NULL) {
		suite_end();
		return;
	}

	printf("  - pixmap Size=%lu Width=%lu Height=%lu Data=%s\n",
		pm->ttp_Size, pm->ttp_Width, pm->ttp_Height,
		pm->ttp_Data ? "non-NULL" : "NULL");

	expect_ulong_ge("ttp_Size", sizeof(struct TT_Pixmap), pm->ttp_Size);
	expect_ulong_ge("ttp_Width", 1, pm->ttp_Width);
	expect_ulong_ge("ttp_Height", 1, pm->ttp_Height);
	expect_ptr_nonnull("ttp_Data", pm->ttp_Data);

	nonzero = 0;
	if (pm->ttp_Data != NULL) {
		for (i = 0; i < pm->ttp_Width * pm->ttp_Height; i++) {
			if (pm->ttp_Data[i] != 0) {
				nonzero++;
			}
		}
	}
	expect_ulong_ge("nonzero pixels", 1, nonzero);

	TT_FreePixmap(pm);
	expect_true("TT_FreePixmap", TRUE);

	suite_end();
}

static void test_encoding_utf8(void)
{
	ULONG count;
	ULONG len_latin;
	ULONG len_utf8;
	struct TagItem set_tags[3];
	/* "café" — 5 bytes, 4 Unicode characters (e-acute is C3 A9) */
	static const UBYTE utf8_cafe[] = {
		'c', 'a', 'f', 0xC3, 0xA9, 0
	};

	/*
	 * TT_Encoding is Set-only (not returned by GetAttrs). Prove UTF-8 by
	 * comparing TextLength: Latin-1 count=5 (one glyph per byte) vs UTF-8
	 * count=4 (codepoints). UTF-8 width must be shorter than Latin-1.
	 */
	suite_begin("UTF-8 encoding path",
		"Set UTF-8 and show TextLength uses codepoint count",
		"TT_SetAttrsA, TT_TextLength, TT_Text");

	if (g_font == NULL) {
		expect_true("skipped (no font)", FALSE);
		suite_end();
		return;
	}

	set_tags[0].ti_Tag = TT_Encoding;
	set_tags[0].ti_Data = TT_Encoding_ISO8859_1;
	set_tags[1].ti_Tag = TAG_DONE;
	set_tags[1].ti_Data = 0;
	TT_SetAttrsA(&g_rp, set_tags);
	len_latin = TT_TextLength(&g_rp, (APTR)utf8_cafe, 5);
	expect_ulong_ge("TextLength Latin-1 (5 bytes)", 1, len_latin);

	set_tags[0].ti_Tag = TT_Encoding;
	set_tags[0].ti_Data = TT_Encoding_UTF8;
	set_tags[1].ti_Tag = TAG_DONE;
	set_tags[1].ti_Data = 0;
	count = TT_SetAttrsA(&g_rp, set_tags);
	expect_ulong_ge("SetAttrs UTF-8", 1, count);

	len_utf8 = TT_TextLength(&g_rp, (APTR)utf8_cafe, 4);
	printf("  - TextLength Latin-1(5)=%lu UTF-8(4)=%lu\n",
		len_latin, len_utf8);
	expect_ulong_ge("TextLength UTF-8 (4 codepoints)", 1, len_utf8);
	expect_true("UTF-8 width < Latin-1 width", len_utf8 < len_latin);

	Move(&g_rp, 8, (WORD)(g_fontsize + 4));
	TT_Text(&g_rp, (APTR)utf8_cafe, 4);
	expect_true("TT_Text UTF-8", TRUE);

	set_tags[0].ti_Tag = TT_Encoding;
	set_tags[0].ti_Data = TT_Encoding_ISO8859_1;
	set_tags[1].ti_Tag = TAG_DONE;
	TT_SetAttrsA(&g_rp, set_tags);

	suite_end();
}

static void test_request(void)
{
	APTR req;
	struct TagItem *attrs;

	suite_begin("TT_AllocRequest / TT_Request / TT_FreeRequest",
		g_do_request
			? "Interactive requester (cancel or OK both OK)"
			: "Allocate/free only (REQUEST=1 enables GUI)",
		"TT_AllocRequest, TT_RequestA, TT_FreeRequest");

	req = TT_AllocRequest();
	expect_ptr_nonnull("TT_AllocRequest", req);
	if (req == NULL) {
		suite_end();
		return;
	}

	if (g_do_request) {
		printf("  - opening font requester (use Cancel or OK)...\n");
		attrs = TT_RequestA(req, NULL);
		printf("  - TT_RequestA -> %s\n",
			attrs ? "taglist (OK)" : "NULL (Cancel/fail)");
		expect_true("TT_RequestA returned without crash", TRUE);
		if (attrs != NULL) {
			APTR font2;
			font2 = TT_OpenFontA(attrs);
			printf("  - OpenFont from requester attrs -> %s\n",
				font2 ? "ok" : "NULL");
			TT_CloseFont(font2);
		}
	} else {
		printf("  - skipped TT_RequestA (pass REQUEST=1 to enable)\n");
		expect_true("alloc/free path covered", TRUE);
	}

	TT_FreeRequest(req);
	expect_true("TT_FreeRequest", TRUE);

	suite_end();
}

static void test_second_open_weight(void)
{
	APTR bold;
	STRPTR family_table[3];
	struct TagItem tags[6];

	suite_begin("Second face open (bold attempt)",
		"Open same family with bold weight; NULL is OK if unavailable",
		"TT_OpenFontA, TT_CloseFont");

	if (g_family[0] == '\0' && g_fontfile[0] != '\0') {
		printf("  - skipped (FONT= path has no family table)\n");
		expect_true("skipped", TRUE);
		suite_end();
		return;
	}

	family_table[0] = g_family[0] ? g_family : "default";
	family_table[1] = "default";
	family_table[2] = NULL;

	tags[0].ti_Tag = TT_FamilyTable;
	tags[0].ti_Data = (ULONG)family_table;
	tags[1].ti_Tag = TT_FontSize;
	tags[1].ti_Data = (ULONG)g_fontsize;
	tags[2].ti_Tag = TT_FontWeight;
	tags[2].ti_Data = TT_FontWeight_Bold;
	tags[3].ti_Tag = TT_FontStyle;
	tags[3].ti_Data = TT_FontStyle_Regular;
	tags[4].ti_Tag = TAG_DONE;
	tags[4].ti_Data = 0;

	bold = TT_OpenFontA(tags);
	printf("  - bold open -> %s\n", bold ? "ok" : "NULL (no bold face)");
	expect_true("OpenFont bold attempt completed", TRUE);
	TT_CloseFont(bold);

	suite_end();
}

/* ----------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	BOOL libs_ok;

	printf("=== TTEngineTest for ttengine.library ===\n\n");
	print_api_map();
	parse_args(argc, argv);

	printf("Options: FONT=%s FAMILY=%s SIZE=%ld REQUEST=%s\n\n",
		g_fontfile[0] ? g_fontfile : "(none)",
		g_family[0] ? g_family : "(auto)",
		g_fontsize,
		g_do_request ? "yes" : "no");

	libs_ok = open_libraries();
	if (!libs_ok) {
		close_libraries();
		return 20;
	}

	if (!create_rastport()) {
		close_libraries();
		return 20;
	}

	test_total = 0;
	test_passed = 0;
	test_failed = 0;

	test_null_safety();
	test_open_failures();
	test_family_list();

	g_font = open_test_font();
	if (g_font == NULL) {
		printf("FATAL: could not open a test font.\n");
		printf("  Provide FONT=SYS:Fonts/TrueType/DejaVuSans.ttf\n");
		printf("  or FAMILY=Arial (with ENV:ttengine.database)\n\n");
		test_total++;
		test_failed++;
		printf("TEST %ld: open test font\n  RESULT: FAIL\n\n", test_total);
	} else {
		test_open_setfont();
		test_attrs();
		test_text_metrics();
		test_text_render();
		test_pixmap();
		test_encoding_utf8();
		test_second_open_weight();
	}

	test_request();

	if (g_font != NULL) {
		TT_CloseFont(g_font);
		g_font = NULL;
	}
	destroy_rastport();
	print_summary();
	close_libraries();

	if (test_failed != 0) {
		return 1;
	}
	return 0;
}
