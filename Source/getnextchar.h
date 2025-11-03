BOOL set_encoding_callback(struct TTEngineBase *ttb, struct RenderEnv *re);

ULONG gnc_default_no_codepage(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_default_codepage(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_custom_table(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_utf32be(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_utf16be(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_utf32le(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_utf16le(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_utf32(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_utf16(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_utf8(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_2(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_3(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_4(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_5(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_6(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_7(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_8(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_9(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_10(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_11(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_13(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_14(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_15(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);
ULONG gnc_iso8859_16(struct TTEngineBase *ttb, struct RenderEnv *re, STRPTR *pointer);

