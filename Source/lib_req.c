/* font requester functions */

#define __NOLIBBASE__

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/layers.h>
#include <proto/ttengine.h>

#include <intuition/sghooks.h>

#include "lib_req.h"

#define PREVIEW_HEIGHT  35

#define FILTER_NONE          6
#define FILTER_FIXEDWIDTH    7

#ifdef __amigaos4__
#define REQ_USEINTUITION struct IntuitionIFace *IIntuition = rd->IIntuition
#define REQ_USEGADTOOLS struct GadToolsIFace *IGadTools = rd->IGadTools
#define REQ_USELAYERS struct LayersIFace *ILayers = rd->ILayers
#else
#define REQ_USEINTUITION struct Library *IntuitionBase = rd->IntuitionBase
#define REQ_USEGADTOOLS struct Library *GadToolsBase = rd->GadToolsBase
#define REQ_USELAYERS struct Library *LayersBase = rd->LayersBase
#endif

/// structures

struct ReqData
  {
    WORD def_left, def_top, def_width, def_height;
    ULONG flags;
    STRPTR title, positive, negative;
    struct Window *window;
    struct Screen *screen;
    struct Gadget *gadlist, *namegad, *sizegad, *sizelv, *namelv;
    struct Gadget *weight, *style;
    APTR visinfo;
    struct Library *IntuitionBase;
    struct Library *GadToolsBase;
    struct Library *DOSBase;
    struct Library *LayersBase;

    #ifdef __amigaos4__
    struct IntuitionIFace *IIntuition;
    struct GadToolsIFace *IGadTools;
    struct DOSIFace *IDOS;
    struct LayersIFace *ILayers;
    #endif

    struct List sizelist;
                struct List filtered_list;
    ULONG namepos, sizepos, nametop, sizetop, stylepos, weightpos;
    LONG initsize;
    STRPTR initname;
    STRPTR weightlist[10];
    UWORD wt[9];
    struct Rectangle pvclip;
  };

#define GADID_OK          9
#define GADID_CANCEL     10
#define GADID_FONTLIST   11
#define GADID_SIZELIST   12
#define GADID_FONTSTR    13
#define GADID_SIZESTR    14
#define GADID_STYLE      15
#define GADID_WEIGHT     16

#define WEIGHT_LABEL     "Weight"
#define STYLE_LABEL      "Style"

struct Node SizeList[11] = {
  {NULL, NULL, 0, 0, "9"},
  {NULL, NULL, 0, 0, "11"},
  {NULL, NULL, 0, 0, "13"},
  {NULL, NULL, 0, 0, "15"},
  {NULL, NULL, 0, 0, "18"},
  {NULL, NULL, 0, 0, "21"},
  {NULL, NULL, 0, 0, "24"},
  {NULL, NULL, 0, 0, "30"},
  {NULL, NULL, 0, 0, "36"},
  {NULL, NULL, 0, 0, "48"},
  {NULL, NULL, 0, 0, "72"}
  };

LONG SizeTab[11] = {9, 11, 13, 15, 18, 21, 24, 30, 36, 48, 72};
STRPTR StyleEntries[3] = {"regular", "italic", NULL};
STRPTR WeightEntries[10] = {"ultralight", "light", "book", "normal",  "medium", "demi", "bold", "heavy", "black", NULL};

#ifdef __amigaos4__
struct TTEngineIFace *ITTEngine_g;
#endif

///
/// ReplaceString()

BOOL ReplaceString(struct TTEngineBase *ttb, STRPTR *place, STRPTR newstring)
{
  USESYSBASE;
  BOOL rv = TRUE;

  LD(kp2("[$%08lx, \"%s\"]", place, (newstring) ? newstring : "NULL");)
  LD(kp2("*place = $%08lx (\"%s\")", *place, (*place) ? *place : "NULL");)

  if (*place)
  {
    LD(kp2("freeing %ld bytes at $%08lx", strlen(*place) + 1, *place);)
    FreePooled(ttb->ttb_MemPool, *place, strlen(*place) + 1);
    *place = NULL;
  }

  if (newstring)
  {
    ULONG len;

    LD(kp("attempt to determine newstring buffer size...");)
    len = strlen(newstring) + 1;
    LD(kp1("...determined (%ld)", len);)

    if ((*place = AllocPooled(ttb->ttb_MemPool, len)))
    {
      LD(kp2("allocated %ld bytes at $%08lx", len, *place);)
      strcpy(*place, newstring);
      LD(kp("string copied to allocated buffer");)
    }
    else rv = FALSE;
  }
  return rv;
}
///
/// InitSize()
// Try to match initial size with one in the listview and then set initial
// listview selection position. If not found, size listview has no selection.

void InitSize(struct ReqData *rd)
{
  int i;

  rd->sizepos = ~0;
  for (i = 0; i < 11; i++)
   if (SizeTab[i] == rd->initsize) rd->sizepos = i;
  LD(kp1("initial size lv pos set to %ld", rd->sizepos);)
  return;
}

///
/// InitName()
void InitName(struct ReqData *rd)
{
  struct Node *n;
  LONG i = ~0, j = 0;

  if (rd->initname)
  {
                for (n = (struct Node*)rd->filtered_list.lh_Head; n->ln_Succ; n = n->ln_Succ)
    {
      if (strcmp(n->ln_Name, rd->initname) == 0)
      {
        i = j;
        break;
      }
      j++;
    }
  }
  else i = 0;

  rd->namepos = i;
  rd->nametop = (i < 0) ? 0 : i;
  return;
}

///
/// open_req_window()

static BOOL open_req_window(struct ReqData *rd)
  {
    REQ_USEINTUITION;
    BOOL result = FALSE;

    LD(kp(""));

    if ((rd->window = OpenWindowTags(NULL,
      WA_CustomScreen, (ULONG)rd->screen,
      WA_Width, rd->def_width,
      WA_Height, rd->def_height,
      WA_Left, rd->def_left,
      WA_Top, rd->def_top,
      WA_Title, (ULONG)rd->title,
      WA_DragBar, TRUE,
      WA_CloseGadget, TRUE,
      WA_DepthGadget, TRUE,
      WA_SizeGadget, TRUE,
      WA_SizeBBottom, TRUE,
      WA_MinWidth, 150,
      WA_MinHeight, 100 + (rd->screen->Height >> 2),
      WA_MaxWidth, rd->screen->Width,
      WA_MaxHeight, rd->screen->Height,
      WA_AutoAdjust, TRUE,
      WA_Activate, (rd->flags & TTRQF_ACTIVATE) ? TRUE : FALSE,
      WA_IDCMP, IDCMP_CLOSEWINDOW |
                IDCMP_REFRESHWINDOW |
                IDCMP_GADGETUP |
                IDCMP_NEWSIZE |
                IDCMP_GADGETDOWN |
                IDCMP_MOUSEMOVE |
                IDCMP_INTUITICKS |
                IDCMP_MOUSEBUTTONS,
    TAG_END))) result = TRUE;

    LD(kp1("requester window opened [$%08lx].", rd->window);)
    return result;
  }

///
/// sizestring_edithook()

#ifdef __amigaos4__

ULONG sizestring_edithook (struct Hook *hook, struct SGWork *sgw, ULONG *msg)
{

#elif __MORPHOS__

ULONG sizestring_edithook(void)
{
  struct SGWork *sgw = (struct SGWork*)REG_A2;
  ULONG *msg = (ULONG*)REG_A1;

#else

__ASM__ __SAVE_DS__ ULONG sizestring_edithook (__REG__(a2, struct SGWork *sgw), __REG__(a1, ULONG *msg))
{

#endif

  ULONG return_code;

  return_code = ~0L;

  if (*msg == SGH_KEY)
  {
    if ((sgw->EditOp == EO_REPLACECHAR) || (sgw->EditOp == EO_INSERTCHAR))
    {
      if ((sgw->Code < '0') || (sgw->Code > '9'))
      {
        sgw->Actions |= SGA_BEEP;
        sgw->Actions &= ~SGA_USE;
      }
    }
  }
  else return_code = 0;

  return(return_code);
}

#ifdef __MORPHOS__

struct EmulLibEntry sizestring_edithook_gate = {TRAP_LIB, 0, (VOID(*)(VOID))sizestring_edithook};

struct Hook h_sizestring_edit = {{NULL, NULL}, (HOOKFUNC)&sizestring_edithook_gate,
 NULL, NULL};

#else

struct Hook h_sizestring_edit = {{NULL, NULL}, (HOOKFUNC)sizestring_edithook,
 NULL, NULL};

#endif

///
/// refresh_weights()

VOID refresh_weights(struct TTEngineBase *ttb, struct ReqData *rd)
{
  USESYSBASE;
  BYTE i;
  STRPTR famname;
  REQ_USEGADTOOLS;
  struct TTFamily *fm;
  struct TTFont *tf;

  LD(kp(".");)
  famname = ((struct StringInfo*)rd->namegad->SpecialInfo)->Buffer;
  if ((fm = (struct TTFamily*)FindName(&ttb->ttb_DataBase, famname)))
  {
    STRPTR *c = rd->weightlist;

    if (fm->alias) fm = fm->alias;
    for (i = 8; i >= 0; i--) rd->wt[i] = 0;
    for (tf = (struct TTFont*)fm->faces.lh_Head; tf->node.ln_Succ; tf = (struct TTFont*)tf->node.ln_Succ)
    {
      rd->wt[tf->weight / 100 - 1] = tf->weight;
    }
    for (i = 0; i < 9; i++) if (rd->wt[i]) *c++ = WeightEntries[i];
    *c = NULL;
    GT_SetGadgetAttrs(rd->weight, rd->window, NULL, GTCY_Labels, (ULONG)&rd->weightlist, TAG_END);
  }
}

///
/// redraw_preview()

void redraw_preview(struct TTEngineBase *ttb, struct ReqData *rd)
{
  USELIB(GfxBase, IGraphics, GraphicsIFace);
  USELIB(DOSBase, IDOS, DOSIFace);
  REQ_USELAYERS;
  REQ_USEGADTOOLS;
  APTR ttfont;
  STRPTR ftable[2] = {NULL, NULL};
  struct Region *rg, *old;

  #ifdef __amigaos4__
  struct TTEngineIFace *ITTEngine = ITTEngine_g;
  #else
  struct Library *TTEngineBase = (struct Library*)ttb;
  #endif

  if (!(rd->flags & TTRQF_DO_PREVIEW)) return;

  if ((rg = NewRegion()))
  {
    if (OrRectRegion(rg, &rd->pvclip))
    {
      ULONG fstyle = TT_FontStyle_Regular;
      ULONG fsize = 24;
      ULONG fweight = TT_FontWeight_Normal;

      if (rd->flags & TTRQF_DO_STYLE)
      {
        GT_GetGadgetAttrs(rd->style, rd->window, NULL,
         GTCY_Active, (ULONG)&fstyle,
        TAG_END);
      }

      if (rd->flags & TTRQF_DO_SIZES)
      {
        StrToLong(((struct StringInfo*)rd->sizegad->SpecialInfo)->Buffer, &fsize);
      }

      if (rd->flags & TTRQF_DO_WEIGHT)
      {
        LONG w;
        BYTE x;

        GT_GetGadgetAttrs(rd->weight, rd->window, NULL,
         GTCY_Active, (ULONG)&w,
        TAG_END);

        for (x = 0; x < 9; x++)
        {
          if (rd->wt[x]) w--;
          if (w < 0) break;
        }
        fweight = rd->wt[x];
      }

      old = InstallClipRegion(rd->window->WLayer, rg);
      EraseRect(rd->window->RPort, 0, 0, rd->window->Width, rd->window->Height);
      ftable[0] = ((struct StringInfo*)rd->namegad->SpecialInfo)->Buffer;
      if ((ttfont = TT_OpenFont(
       TT_FamilyTable, (ULONG)ftable,
       TT_FontSize, fsize,
       TT_FontStyle, fstyle,
       TT_FontWeight, fweight,
      TAG_END)))
      {
        TT_SetAttrs(rd->window->RPort,
         TT_Window, (ULONG)rd->window,
        TAG_END);
        TT_SetFont(rd->window->RPort, ttfont);
        Move(rd->window->RPort, rd->pvclip.MinX + 3, rd->pvclip.MaxY - 6);
        SetAPen(rd->window->RPort, 1);
        TT_Text(rd->window->RPort, "The quick brown fox jumps over lazy dog.", 40);
        TT_DoneRastPort(rd->window->RPort);
        TT_CloseFont(ttfont);
      }
      InstallClipRegion(rd->window->WLayer, old);
    }
    DisposeRegion(rg);
  }
}
///
/// layout_gads()

static BOOL layout_gads(struct TTEngineBase *ttb, struct ReqData *rd)
{
  REQ_USEINTUITION;
  REQ_USEGADTOOLS;
  USELIB(GfxBase, IGraphics, GraphicsIFace);
  USELIB(DOSBase, IDOS, DOSIFace);
  BOOL result = FALSE;
  UWORD winw, winh, offx, offy, spc, txth, namew, offcyc = 0;
  struct Window *win = rd->window;
  struct Gadget *g;
  struct NewGadget ng;

  LD(kp(""));
  txth = win->WScreen->RastPort.TxHeight;
  spc = txth >> 2;

  winw = win->Width - win->BorderLeft - win->BorderRight - spc - spc;
  winh = win->Height - win->BorderTop - win->BorderBottom - spc - spc;
  offx = win->BorderLeft + spc;
  offy = win->BorderTop + spc;

  if (rd->gadlist)
  {
    if (rd->namelv) GT_GetGadgetAttrs(rd->namelv, win, NULL,
     GTLV_Selected, (ULONG)&rd->namepos,
     GTLV_Top,      (ULONG)&rd->nametop,
    TAG_END);

    if (rd->sizelv) GT_GetGadgetAttrs(rd->sizelv, win, NULL,
     GTLV_Selected, (ULONG)&rd->sizepos,
     GTLV_Top,      (ULONG)&rd->sizetop,
    TAG_END);

    if (rd->sizegad)
     StrToLong(((struct StringInfo*)rd->sizegad->SpecialInfo)->Buffer, &rd->initsize);

    if (rd->namegad)
     ReplaceString(ttb, &rd->initname, ((struct StringInfo*)rd->namegad->SpecialInfo)->Buffer);

    if (rd->style) GT_GetGadgetAttrs(rd->style, win, NULL,
     GTCY_Active,   (ULONG)&rd->stylepos,
    TAG_END);

    if (rd->weight) GT_GetGadgetAttrs(rd->weight, win, NULL,
     GTCY_Active,   (ULONG)&rd->weightpos,
    TAG_END);

    RemoveGList(win, rd->gadlist, -1);
    FreeGadgets(rd->gadlist);
    rd->gadlist = NULL;

    EraseRect(win->RPort, win->BorderLeft, win->BorderTop, win->Width - win->BorderRight - 1,
      win->Height - win->BorderBottom - 1);

    RefreshWindowFrame(win);
  }

  g = CreateContext(&rd->gadlist);

  /* 'OK' button */

  ng.ng_TextAttr = win->WScreen->Font;
  ng.ng_VisualInfo = rd->visinfo;
  ng.ng_Width = winw * 3 >> 3;
  ng.ng_Height = txth + spc + 2;
  ng.ng_LeftEdge = offx;
  ng.ng_TopEdge = offy + winh - ng.ng_Height;
  ng.ng_GadgetText = rd->positive;
  ng.ng_GadgetID = GADID_OK;
  ng.ng_Flags = 0;
  g = CreateGadget(BUTTON_KIND, g, &ng, TAG_END);

  /* 'Cancel' button */

  ng.ng_LeftEdge = offx + winw - ng.ng_Width;
  ng.ng_GadgetText = rd->negative;
  ng.ng_GadgetID = GADID_CANCEL;
  g = CreateGadget(BUTTON_KIND, g, &ng, TAG_END);

	winh -= ng.ng_Height + spc;  // up of gadget height and space above
  namew = winw;

  /* preview field */

  if (rd->flags & TTRQF_DO_PREVIEW)
  {
		DrawBevelBox(rd->window->RPort, offx, offy + winh - PREVIEW_HEIGHT, winw, PREVIEW_HEIGHT,
     GTBB_Recessed, TRUE,
     GTBB_FrameType, BBFT_BUTTON,
     GT_VisualInfo, (ULONG)rd->visinfo,
    TAG_END);

    rd->pvclip.MinX = offx + 2;
    rd->pvclip.MaxX = offx + winw - 3;
		rd->pvclip.MinY = offy + winh - PREVIEW_HEIGHT + 1;
		rd->pvclip.MaxY = offy + winh - 2;

		winh -= PREVIEW_HEIGHT + spc;  // up of gadget heigth and space above;
  }
	ng.ng_TopEdge = offy + winh;

	/* cycles horizontal shift */

  if (rd->flags & TTRQF_DO_STYLE) offcyc = spc + txth + TextLength(win->RPort, STYLE_LABEL, strlen(STYLE_LABEL));
  if (rd->flags & TTRQF_DO_WEIGHT)
  {
    UWORD offn;

    offn = spc + txth + TextLength(win->RPort, WEIGHT_LABEL, strlen(WEIGHT_LABEL));
    if (offn > offcyc) offcyc = offn;
  }

  /* Font style cycle */

  if (rd->flags & TTRQF_DO_STYLE)
  {
		ng.ng_Height = txth + spc + 2;
    ng.ng_LeftEdge = offcyc;
		ng.ng_TopEdge -= ng.ng_Height;
    ng.ng_Width = winw - offcyc + spc;
    ng.ng_GadgetText = "Style";
    ng.ng_GadgetID = GADID_STYLE;
    g = CreateGadget(CYCLE_KIND, g, &ng,
     GTCY_Labels, (ULONG)&StyleEntries,
     GTCY_Active, rd->stylepos,
    TAG_END);
    rd->style = g;
		winh -= ng.ng_Height + spc;
  }

  /* Font weight cycle */

  if (rd->flags & TTRQF_DO_WEIGHT)
  {
    ng.ng_LeftEdge = offcyc;
    ng.ng_TopEdge -= ng.ng_Height + spc;
    ng.ng_Width = winw - offcyc + spc;
    ng.ng_GadgetText = "Weight";
    ng.ng_GadgetID = GADID_WEIGHT;
    g = CreateGadget(CYCLE_KIND, g, &ng,
     GTCY_Labels, (ULONG)&WeightEntries,
     GTCY_Active, rd->weightpos,
    TAG_END);
    rd->weight = g;
    winh -= ng.ng_Height + spc;
  }

  ng.ng_Height = txth + spc + 2;
	ng.ng_TopEdge -= spc + spc + ng.ng_Height;
  ng.ng_GadgetText = NULL;

  /* Font size string */

  if (rd->flags & TTRQF_DO_SIZES)
  {
    ng.ng_Width = 16 + TextLength(win->RPort, "000", 3) + spc + 4;
    ng.ng_LeftEdge = offx + winw - ng.ng_Width;
    ng.ng_GadgetID = GADID_SIZESTR;
    g = CreateGadget(STRING_KIND, g, &ng,
     GTST_EditHook, (ULONG)&h_sizestring_edit,
     GTST_MaxChars, 3,
    TAG_END);
    rd->sizegad = g;
    namew -= ng.ng_Width + spc;
    LD(kp1("font size string at $%08lx", g));
  }

  /* Font name string */

  ng.ng_LeftEdge = offx;
  ng.ng_Width = namew;
  ng.ng_GadgetID = GADID_FONTSTR;
  g = CreateGadget(STRING_KIND, g, &ng, TAG_END);
  rd->namegad = g;
  namew = winw;
  LD(kp1("font name string at $%08lx", g));

  ng.ng_TopEdge = offy;
  ng.ng_Height = winh;

  /* Font size listview */

  if (rd->flags & TTRQF_DO_SIZES)
  {
    ng.ng_Width = 16 + TextLength(win->RPort, "000", 3) + spc + 4;
    ng.ng_LeftEdge = offx + winw - ng.ng_Width;
    ng.ng_GadgetID = GADID_SIZELIST;
    g = CreateGadget(LISTVIEW_KIND, g, &ng,
      GTLV_Labels,        (ULONG)&rd->sizelist,
      GTLV_ShowSelected,  (ULONG)rd->sizegad,
      GTLV_Selected,      rd->sizepos,
      GTLV_Top,           rd->sizetop,
    TAG_END);
    rd->sizelv = g;
    namew -= ng.ng_Width + spc;
    LD(kp1("font size list at $%08lx", g));
  }

  /* Font name listview */

  ng.ng_LeftEdge = offx;
  ng.ng_Width = namew;
  LD(kp2("nlv dimensions: %ld x %ld", ng.ng_Width, ng.ng_Height));
  LD(kp2("nlv position: (%ld, %ld)", ng.ng_LeftEdge, ng.ng_TopEdge));
  ng.ng_GadgetID = GADID_FONTLIST;
  g = CreateGadget(LISTVIEW_KIND, g, &ng,
                GTLV_Labels,        (ULONG)&rd->filtered_list,
    GTLV_ShowSelected,  (ULONG)rd->namegad,
    GTLV_Selected,      rd->namepos,
    GTLV_Top,           rd->nametop,
  TAG_END);
  rd->namelv = g;
  LD(kp1("font name list at $%08lx", g));

  LD(kp1("all gads created, last is $%08lx", g));
  if (g)
  {
    LD(kp1("initname = '%s'", rd->initname);)
    AddGList(win, rd->gadlist, -1, -1, NULL);
    RefreshGList(rd->gadlist, win, NULL, -1);
    GT_RefreshWindow(win, NULL);
    GT_SetGadgetAttrs(rd->sizegad, win, NULL, GTIN_Number, rd->initsize, TAG_END);
    GT_SetGadgetAttrs(rd->sizelv, win, NULL, GTLV_Top, rd->sizetop, TAG_END);
    GT_SetGadgetAttrs(rd->namelv, win, NULL, GTLV_Top, rd->nametop, TAG_END);
    GT_SetGadgetAttrs(rd->namegad, win, NULL, GTST_String, (ULONG)rd->initname, TAG_END);
    if (rd->flags & TTRQF_DO_WEIGHT) refresh_weights(ttb, rd);
    if (rd->flags & TTRQF_DO_PREVIEW) redraw_preview(ttb, rd);

    result = TRUE;
  }
  return result;
}

///
/// get_my_vinfo()

static BOOL get_my_vinfo(struct ReqData *rd)
  {
    REQ_USEGADTOOLS;
    BOOL result = FALSE;

    LD(kp("."));
    if ((rd->visinfo = GetVisualInfo(rd->window->WScreen, TAG_END)))
     result = TRUE;
    return result;
  }

///
/// free_my_vinfo()

static VOID free_my_vinfo(struct ReqData *rd)
  {
    REQ_USEGADTOOLS;

    LD(kp("."));
    FreeVisualInfo(rd->visinfo);
    return;
  }

///
/// dispose_gads()

static VOID dispose_gads(struct ReqData *rd)
  {
    REQ_USEGADTOOLS;
    REQ_USEINTUITION;

    LD(kp("."));
    if (rd->gadlist)
      {
        RemoveGList(rd->window, rd->gadlist, -1);
        FreeGadgets(rd->gadlist);
      }
    return;
  }

///
/// main_loop()

static BOOL main_loop(struct TTEngineBase *ttb, struct ReqData *rd)
{
  USESYSBASE;
  REQ_USEGADTOOLS;
  BOOL running = TRUE, result = FALSE;
  ULONG msgclass;
  struct IntuiMessage *imsg;

  LD(kp("."));
  if (rd->flags & TTRQF_DO_WEIGHT) refresh_weights(ttb, rd);

  while (running)
  {
    WaitPort(rd->window->UserPort);
    while ((imsg = GT_GetIMsg(rd->window->UserPort)))
    {
      struct Gadget *gad;

      gad = (struct Gadget*)imsg->IAddress;
      msgclass = imsg->Class;
      GT_ReplyIMsg(imsg);
      switch (msgclass)
      {
        case IDCMP_CLOSEWINDOW:
          running = FALSE;
        break;

        case IDCMP_REFRESHWINDOW:
          GT_BeginRefresh(rd->window);
          GT_EndRefresh(rd->window, TRUE);
        break;

        case IDCMP_GADGETUP:
          switch (gad->GadgetID)
          {
            case GADID_CANCEL: running = FALSE; break;
            case GADID_OK: running = FALSE; result = TRUE; break;
            case GADID_FONTLIST:
            {
              if (rd->flags & TTRQF_DO_WEIGHT) refresh_weights(ttb, rd);
              if (rd->flags & TTRQF_DO_PREVIEW) redraw_preview(ttb, rd);
            }
            break;
            case GADID_STYLE:
            case GADID_WEIGHT:
            case GADID_SIZESTR:
            case GADID_SIZELIST: redraw_preview(ttb, rd); break;
          }
        break;

        case IDCMP_NEWSIZE:
          if (!(layout_gads(ttb, rd))) running = FALSE;
        break;
      }
    }
  }
  return result;
}

///
/// init_size_list()

static VOID init_size_list(struct TTEngineBase *ttb, struct ReqData *rd)
  {
    USESYSBASE;
    WORD i;

    LD(kp("."));
    rd->sizelist.lh_Head = (struct Node*)&rd->sizelist.lh_Tail;
    rd->sizelist.lh_Tail = NULL;
    rd->sizelist.lh_TailPred = (struct Node*)&rd->sizelist.lh_Head;
    for (i = 10; i >= 0; i--) AddHead(&rd->sizelist, &SizeList[i]);
    return;
  }

///
/// create_filtered_list()

BOOL create_filtered_list(struct TTEngineBase *ttb, struct ReqData *rdat, ULONG filter)
{
        USESYSBASE;
        struct TTFamily *ttf, *nttf;
        BOOL rv = TRUE, passed;

        /* init list first */

        rdat->filtered_list.lh_Head = (struct Node*)&rdat->filtered_list.lh_Tail;
        rdat->filtered_list.lh_Tail = NULL;
        rdat->filtered_list.lh_TailPred = (struct Node*)&rdat->filtered_list.lh_Head;

        /* copy and filter entries */

        for (ttf = (struct TTFamily*)ttb->ttb_DataBase.lh_Head; ttf->node.ln_Succ;
                ttf = (struct TTFamily*)ttf->node.ln_Succ)
        {
                passed = TRUE;

                switch (filter)
                {
                        case FILTER_NONE: break;

                        case FILTER_FIXEDWIDTH:         // check all fonts of family
                        {
                                struct TTFont *ttfa;

                                for (ttfa = (struct TTFont*)ttf->faces.lh_Head; ttfa->node.ln_Succ;
                                        ttfa = (struct TTFont*)ttfa->node.ln_Succ)
                                {
                                        if (!ttfa->fixed) passed = FALSE;
                                }
                        }
                        break;
                }

                if (passed)             /* allocate new struct TTFamily, copy, attach to filtered_list */
                {
                        if (nttf = AllocPooled(ttb->ttb_MemPool, sizeof(struct TTFamily)))
                        {
                                CopyMem(ttf, nttf, sizeof(struct TTFamily));
                                AddTail(&rdat->filtered_list, &nttf->node);
                        }
                        else       /* exit loop and return false if memory allocation failed */
                        {
                                rv = FALSE;
                                break;
                        }
                }
        }

        return rv;
}

///
/// dispose_filtered_list()

void dispose_filtered_list(struct TTEngineBase *ttb, struct ReqData *rdat)
{
        USESYSBASE;
        struct TTFamily *ttf;

        while (ttf = (struct TTFamily*)RemTail(&rdat->filtered_list))
                FreePooled(ttb->ttb_MemPool, ttf, sizeof(struct TTFamily));
}

///
/// tt_AllocRequest()

#ifdef __amigaos4__

APTR VARARGS68K _TTEngine_TT_AllocRequest(struct TTEngineIFace *Self)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;

#elif __MORPHOS__

struct TTRequest *tt_AllocRequest(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;

#else

__ASM__ __SAVE_DS__ struct TTRequest *tt_AllocRequest(
	__REG__(a6, struct TTEngineBase *ttb))
{

#endif

  USESYSBASE;
  struct TTRequest *ttrq = NULL;
  BOOL success = FALSE;

  LD(kp(""));
  if ((ttrq = (struct TTRequest*)AllocPooled(ttb->ttb_MemPool, sizeof(struct TTRequest))))
  {
    ttrq->ttrq_NameBuffer = NULL;
    ttrq->ttrq_InitialSize = 14;
    ttrq->ttrq_InitialName = NULL;
    ttrq->ttrq_InitialStyle = TT_FontStyle_Regular;
    ttrq->ttrq_InitialLeft = 0;
    ttrq->ttrq_InitialTop = 0;
    ttrq->ttrq_InitialWidth = 0;
    ttrq->ttrq_InitialHeight = 0;

    ttrq->ttrq_Result[0].ti_Tag = TT_FamilyTable;
    ttrq->ttrq_Result[0].ti_Data = (ULONG)&ttrq->ttrq_FamilyTable;

    ttrq->ttrq_Result[1].ti_Tag = TAG_IGNORE;
    ttrq->ttrq_Result[2].ti_Tag = TAG_IGNORE;
    ttrq->ttrq_Result[3].ti_Tag = TAG_IGNORE;

    ttrq->ttrq_Result[4].ti_Tag = TAG_END;
    ttrq->ttrq_Result[4].ti_Data = 0;

                success = TRUE;
  }

  if (success)
  {
    LD(kp1("returns $%08lx.", ttrq));
    return ttrq;
  }
  else
  {
    if (ttrq) FreePooled(ttb->ttb_MemPool, ttrq, sizeof(struct TTRequest));
    return NULL;
  }
}

///
/// tt_RequestA()

#ifdef __amigaos4__

#ifdef __USE_INLINE__
#undef TT_RequestA
#endif

struct TagItem * VARARGS68K _TTEngine_TT_Request(struct TTEngineIFace *Self,
       APTR request,
       ...)
{

    va_list ap;
    struct TagItem *tags;

    va_startlinear(ap, request);
    tags = va_getlinearva(ap, struct TagItem *);

    return Self->TT_RequestA(
        request,
        tags);
}

struct TagItem * VARARGS68K _TTEngine_TT_RequestA(struct TTEngineIFace *Self,
       APTR request1,
       struct TagItem * taglist)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;
  struct TTRequest *request = (struct TTRequest*)request1;

#elif __MORPHOS__

struct TagItem *tt_RequestA(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
  struct TTRequest *request = (struct TTRequest*)REG_A0;
  struct TagItem *taglist = (struct TagItem*)REG_A1;

#else

__ASM__ __SAVE_DS__ struct TagItem *tt_RequestA(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a0, struct TTRequest *request),
	__REG__(a1, struct TagItem *taglist))
{

#endif

  USELIB(UtilityBase, IUtility, UtilityIFace);
  USESYSBASE;
  BOOL request_ok = FALSE;
  STRPTR pubscreen_name;
  struct Window *parent_window;
  struct Screen *locked = NULL;
  struct TagItem empty[] = {{TAG_END, 0}};
  struct TagItem *result = NULL;
  struct ReqData rd;
  struct Library *GadToolsBase = NULL;

  #ifdef __amigaos4__
  struct GadToolsIFace *IGadTools = NULL;
  ITTEngine_g = Self;
  #endif

  LD(kp("."));

  if (!request) return NULL;
  if (!taglist) taglist = empty;

  rd.flags = 0;
  rd.gadlist = NULL;
  rd.namelv = NULL;
  rd.sizelv = NULL;
  rd.style = NULL;
  rd.weight = NULL;
  rd.namepos = 0;
  rd.sizepos = 0;
  rd.nametop = 0;
  rd.sizetop = 0;
  rd.weightpos = 0;
  rd.stylepos = 0;
  rd.initname = NULL;
  init_size_list(ttb, &rd);

  if ((rd.LayersBase = OpenLibrary("layers.library", 39)))
  {
    #ifdef __amigaos4__
    rd.ILayers = (struct LayersIFace *) GetInterface(rd.LayersBase, "main", 1, NULL);
    #endif

    if ((rd.IntuitionBase = OpenLibrary("intuition.library", 39)))
    {
      BOOL set_default_dimensions = FALSE;
                        ULONG filter = FILTER_NONE;

      #ifdef __amigaos4__
      struct IntuitionIFace *IIntuition = (struct IntuitionIFace *) GetInterface(rd.IntuitionBase, "main", 1, NULL);
      rd.IIntuition = IIntuition;
      #else
      struct Library *IntuitionBase = rd.IntuitionBase;
      #endif

      LD(kp1("IntuitionBase = $%08lx.", rd.IntuitionBase);)

      /* determining a screen to open on */

      if (!(rd.screen = (struct Screen*)GetTagData(TTRQ_Screen, 0, taglist)))
      {
        if ((pubscreen_name = (STRPTR)GetTagData(TTRQ_PubScreenName, 0, taglist)))
        {
          if ((locked = LockPubScreen(pubscreen_name))) rd.screen = locked;
        }
        else
        {
          if ((parent_window = (struct Window*)GetTagData(TTRQ_Window, 0, taglist)))
            rd.screen = parent_window->WScreen;
        }
      }

      if (!rd.screen)
      {
        if (!(locked = LockPubScreen(NULL))) return NULL;   /* no screen to open on */
        else rd.screen = locked;
      }

      LD(kp1("Opening on screen $%08lx.", rd.screen);)

      /* determine window dimensions */
      /* set default values only if width and height are 0 (it means request */
      /* structure is used the first time. */

      if (request->ttrq_InitialWidth == 0 && request->ttrq_InitialHeight == 0)
        set_default_dimensions = TRUE;

      if (set_default_dimensions)
      {
        request->ttrq_InitialWidth = rd.screen->Width >> 2;
        if (request->ttrq_InitialWidth < 200) request->ttrq_InitialWidth = 200;
        request->ttrq_InitialHeight = rd.screen->Height >> 1;
        if (request->ttrq_InitialHeight < 200) request->ttrq_InitialHeight = 200;
        request->ttrq_InitialLeft = (rd.screen->Width - rd.def_width) >> 1;
        request->ttrq_InitialTop = (rd.screen->Height - rd.def_height) >> 1;
      }

      /* Override with user specified values. */

      rd.def_width = GetTagData(TTRQ_InitialWidth, request->ttrq_InitialWidth, taglist);
      rd.def_height = GetTagData(TTRQ_InitialHeight, request->ttrq_InitialHeight, taglist);

      /* Minimum window size is 150 x 200, maximum is screen size. Let's clip. */

      if (rd.def_width < 150) rd.def_width = 150;
      if (rd.def_width > rd.screen->Width) rd.def_width = rd.screen->Width;
      if (rd.def_height < 100 + (rd.screen->Height >> 2)) rd.def_height = 100 + (rd.screen->Height >> 2);
      if (rd.def_height > rd.screen->Height) rd.def_height = rd.screen->Height;

      LD(kp2("Window size set to %ld x %ld", rd.def_width, rd.def_height);)

      /* Set default position if request used the first time. */

      if (set_default_dimensions)
      {
        request->ttrq_InitialLeft = (rd.screen->Width - rd.def_width) >> 1;
        request->ttrq_InitialTop = (rd.screen->Height - rd.def_height) >> 1;
      }

      /* Override with user specified values. No need for clipping as window uses */
      /* WA_AutoAdjust. */

      rd.def_left = GetTagData(TTRQ_InitialLeftEdge, request->ttrq_InitialLeft, taglist);
      rd.def_top = GetTagData(TTRQ_InitialTopEdge, request->ttrq_InitialTop, taglist);

      LD(kp2("Window position set to (%ld, %ld)", rd.def_left, rd.def_top);)

      /* texts */

      rd.title = (STRPTR)GetTagData(TTRQ_TitleText, (ULONG)"Select TrueType font", taglist);
      rd.positive = (STRPTR)GetTagData(TTRQ_PositiveText, (ULONG)"OK", taglist);
      rd.negative = (STRPTR)GetTagData(TTRQ_NegativeText, (ULONG)"Cancel", taglist);

      LD(kp("texts initialized");)

      /* flags */

      if (GetTagData(TTRQ_DoSizes, TRUE, taglist)) rd.flags |= TTRQF_DO_SIZES;
      if (GetTagData(TTRQ_DoStyle, FALSE, taglist)) rd.flags |= TTRQF_DO_STYLE;
      if (GetTagData(TTRQ_DoWeight, FALSE, taglist)) rd.flags |= TTRQF_DO_WEIGHT;
      if (GetTagData(TTRQ_Activate, TRUE, taglist)) rd.flags |= TTRQF_ACTIVATE;
      if (GetTagData(TTRQ_DoPreview, FALSE, taglist)) rd.flags |= TTRQF_DO_PREVIEW;

      LD(kp("flags initialized");)

      /* initials */

      rd.initsize = GetTagData(TTRQ_InitialSize, request->ttrq_InitialSize, taglist);
      InitSize(&rd);

                        /* set filter type */

                        if (GetTagData(TTRQ_FixedWidthOnly, FALSE, taglist)) filter = FILTER_FIXEDWIDTH;

      /* opening requester window */

      if ((rd.DOSBase = OpenLibrary("dos.library", 36)))
      {
        #ifdef __amigaos4__
        struct DOSIFace *IDOS = (struct DOSIFace *) GetInterface(rd.DOSBase, "main", 1, NULL);
        rd.IDOS = IDOS;
        #else
        struct Library *DOSBase = rd.DOSBase;
        #endif

        LD(kp1("DOSBase at $%08lx", DOSBase);)

        if ((rd.GadToolsBase = OpenLibrary("gadtools.library", 38)))
        {
          #ifdef __amigaos4__
          IGadTools = (struct GadToolsIFace *) GetInterface(rd.GadToolsBase, "main", 1, NULL);
          rd.IGadTools = IGadTools;
          #else
          GadToolsBase = rd.GadToolsBase;
          #endif

          LD(kp1("GadToolsBase at $%08lx", GadToolsBase);)

                                        if (create_filtered_list(ttb, &rd, filter))
                                        {
                                                STRPTR inname = request->ttrq_InitialName;

                                                if (!inname) inname = rd.filtered_list.lh_Head->ln_Name;
                                                ReplaceString(ttb, &rd.initname, (STRPTR)GetTagData(TTRQ_InitialName,   (ULONG)inname, taglist));
                                                InitName(&rd);

                  if (open_req_window(&rd))
                  {
                    if (get_my_vinfo(&rd))
                    {
                      if (layout_gads(ttb, &rd))
                      {
                        if ((request_ok = main_loop(ttb, &rd)))
                        {
                          ULONG font_size = 0;
                          STRPTR font_name;
                                                         
                          font_name = ((struct StringInfo*)rd.namegad->SpecialInfo)->Buffer;
                          if (ReplaceString(ttb, &request->ttrq_NameBuffer, font_name))
                          {
                            request->ttrq_FamilyTable[0] = request->ttrq_NameBuffer;

                            if (rd.flags & TTRQF_DO_SIZES)
                            {
                              StrToLong(((struct StringInfo*)rd.sizegad->SpecialInfo)->Buffer, &font_size);
                              request->ttrq_Result[1].ti_Data = font_size;
                              request->ttrq_Result[1].ti_Tag = TT_FontSize;
                            }

                            if (rd.flags & TTRQF_DO_STYLE)
                            {
                              ULONG s;

                              GT_GetGadgetAttrs(rd.style, rd.window, NULL,
                               GTCY_Active, (ULONG)&s,
                              TAG_END);
                              request->ttrq_Result[2].ti_Data = s;
                              request->ttrq_Result[2].ti_Tag = TT_FontStyle;
                            }

                            if (rd.flags & TTRQF_DO_WEIGHT)
                            {
                              LONG w;
                              BYTE x;

                              GT_GetGadgetAttrs(rd.weight, rd.window, NULL,
                               GTCY_Active, (ULONG)&w,
                              TAG_END);

                              for (x = 0; x < 9; x++)
                              {
                                if (rd.wt[x]) w--;
                                if (w < 0) break;
                              }
                              request->ttrq_Result[3].ti_Data = rd.wt[x];
                              request->ttrq_Result[3].ti_Tag = TT_FontWeight;
                            }
                            result = request->ttrq_Result;
                          }
                        }
                        ReplaceString(ttb, &request->ttrq_InitialName,
                         ((struct StringInfo*)rd.namegad->SpecialInfo)->Buffer);

                        if (rd.flags & TTRQF_DO_SIZES)
                        {
                          StrToLong(((struct StringInfo*)rd.sizegad->SpecialInfo)->Buffer,
                           &request->ttrq_InitialSize);
                        }
                      }
                      dispose_gads(&rd);
                      free_my_vinfo(&rd);
                    }
                    request->ttrq_InitialWidth = rd.window->Width;
                    request->ttrq_InitialHeight = rd.window->Height;
                    request->ttrq_InitialLeft = rd.window->LeftEdge;
                    request->ttrq_InitialTop = rd.window->TopEdge;
                    CloseWindow(rd.window);
                  }
                                                dispose_filtered_list(ttb, &rd);
                                        }

          #ifdef __amigaos4__
          DropInterface((struct Interface *) rd.IGadTools);
          #endif
          CloseLibrary(rd.GadToolsBase);
        }

        #ifdef __amigaos4__
        DropInterface((struct Interface *) rd.IDOS);
        #endif
        CloseLibrary(rd.DOSBase);
      }
      if (locked) UnlockPubScreen(NULL, locked);

      #ifdef __amigaos4__
      DropInterface((struct Interface *) rd.IIntuition);
      #endif
      CloseLibrary(rd.IntuitionBase);
    }

    #ifdef __amigaos4__
    DropInterface((struct Interface *) rd.ILayers);
    #endif
    CloseLibrary(rd.LayersBase);
  }
  ReplaceString(ttb, &rd.initname, NULL);
  return result;
}

///
/// tt_FreeRequest()

#ifdef __amigaos4__

VOID VARARGS68K _TTEngine_TT_FreeRequest(struct TTEngineIFace *Self,
       APTR request)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)Self->Data.LibBase;
  struct TTRequest *ttrq = (struct TTRequest*)request;

#elif __MORPHOS__

void tt_FreeRequest(void)
{
  struct TTEngineBase *ttb = (struct TTEngineBase*)REG_A6;
  struct TTRequest *ttrq = (struct TTRequest*)REG_A0;

#else

__ASM__ __SAVE_DS__ void tt_FreeRequest(
	__REG__(a6, struct TTEngineBase *ttb),
	__REG__(a0, struct TTRequest *ttrq))
{

#endif

  USESYSBASE;

  LD(kp(""));
  if (ttrq)
  {
    ReplaceString(ttb, &ttrq->ttrq_InitialName, NULL);
    ReplaceString(ttb, &ttrq->ttrq_NameBuffer, NULL);
    FreePooled(ttb->ttb_MemPool, ttrq, sizeof(struct TTRequest));
  }

  return;
}

///


