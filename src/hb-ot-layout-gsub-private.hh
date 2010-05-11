/*
 * Copyright (C) 2007,2008,2009,2010  Red Hat, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_GSUB_PRIVATE_HH
#define HB_OT_LAYOUT_GSUB_PRIVATE_HH

#include "hb-ot-layout-gsubgpos-private.hh"


#undef BUFFER
#define BUFFER context->buffer


struct SingleSubstFormat1
{
  friend struct SingleSubst;

  private:

  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    hb_codepoint_t glyph_id = IN_CURGLYPH ();
    unsigned int index = (this+coverage) (glyph_id);
    if (likely (index == NOT_COVERED))
      return false;

    glyph_id += deltaGlyphID;
    _hb_buffer_replace_glyph (context->buffer, glyph_id);

    /* We inherit the old glyph class to the substituted glyph */
    if (_hb_ot_layout_has_new_glyph_classes (context->layout->face))
      _hb_ot_layout_set_glyph_property (context->layout->face, glyph_id, context->property);

    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return coverage.sanitize (context, this)
	&& deltaGlyphID.sanitize (context);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  SHORT		deltaGlyphID;		/* Add to original GlyphID to get
					 * substitute GlyphID */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct SingleSubstFormat2
{
  friend struct SingleSubst;

  private:

  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    hb_codepoint_t glyph_id = IN_CURGLYPH ();
    unsigned int index = (this+coverage) (glyph_id);
    if (likely (index == NOT_COVERED))
      return false;

    if (unlikely (index >= substitute.len))
      return false;

    glyph_id = substitute[index];
    _hb_buffer_replace_glyph (context->buffer, glyph_id);

    /* We inherit the old glyph class to the substituted glyph */
    if (_hb_ot_layout_has_new_glyph_classes (context->layout->face))
      _hb_ot_layout_set_glyph_property (context->layout->face, glyph_id, context->property);

    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return coverage.sanitize (context, this)
	&& substitute.sanitize (context);
  }

  private:
  USHORT	format;			/* Format identifier--format = 2 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  ArrayOf<GlyphID>
		substitute;		/* Array of substitute
					 * GlyphIDs--ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, substitute);
};

struct SingleSubst
{
  friend struct SubstLookupSubTable;

  private:

  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (context);
    case 2: return u.format2.apply (context);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (context)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (context);
    case 2: return u.format2.sanitize (context);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  SingleSubstFormat1	format1;
  SingleSubstFormat2	format2;
  } u;
};


struct Sequence
{
  friend struct MultipleSubstFormat1;

  private:
  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    if (unlikely (!substitute.len))
      return false;

    _hb_buffer_add_output_glyphs_be16 (context->buffer, 1,
				       substitute.len, (const uint16_t *) substitute.array,
				       0xFFFF, 0xFFFF);

    /* This is a guess only ... */
    if (_hb_ot_layout_has_new_glyph_classes (context->layout->face))
    {
      unsigned int property = context->property;
      if (property == HB_OT_LAYOUT_GLYPH_CLASS_LIGATURE)
        property = HB_OT_LAYOUT_GLYPH_CLASS_BASE_GLYPH;

      unsigned int count = substitute.len;
      for (unsigned int n = 0; n < count; n++)
	_hb_ot_layout_set_glyph_property (context->layout->face, substitute[n], property);
    }

    return true;
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return substitute.sanitize (context);
  }

  private:
  ArrayOf<GlyphID>
		substitute;		/* String of GlyphIDs to substitute */
  public:
  DEFINE_SIZE_ARRAY (2, substitute);
};

struct MultipleSubstFormat1
{
  friend struct MultipleSubst;

  private:

  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();

    unsigned int index = (this+coverage) (IN_CURGLYPH ());
    if (likely (index == NOT_COVERED))
      return false;

    return (this+sequence[index]).apply (context);
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return coverage.sanitize (context, this)
	&& sequence.sanitize (context, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  OffsetArrayOf<Sequence>
		sequence;		/* Array of Sequence tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, sequence);
};

struct MultipleSubst
{
  friend struct SubstLookupSubTable;

  private:

  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (context);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (context)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (context);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  MultipleSubstFormat1	format1;
  } u;
};


typedef ArrayOf<GlyphID> AlternateSet;	/* Array of alternate GlyphIDs--in
					 * arbitrary order */

struct AlternateSubstFormat1
{
  friend struct AlternateSubst;

  private:

  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    hb_codepoint_t glyph_id = IN_CURGLYPH ();

    unsigned int index = (this+coverage) (glyph_id);
    if (likely (index == NOT_COVERED))
      return false;

    const AlternateSet &alt_set = this+alternateSet[index];

    if (unlikely (!alt_set.len))
      return false;

    unsigned int alt_index = 0;

    /* XXX callback to user to choose alternate
    if (context->layout->face->altfunc)
      alt_index = (context->layout->face->altfunc)(context->layout->layout, context->buffer,
				    context->buffer->out_pos, glyph_id,
				    alt_set.len, alt_set.array);
				   */

    if (unlikely (alt_index >= alt_set.len))
      return false;

    glyph_id = alt_set[alt_index];

    _hb_buffer_replace_glyph (context->buffer, glyph_id);

    /* We inherit the old glyph class to the substituted glyph */
    if (_hb_ot_layout_has_new_glyph_classes (context->layout->face))
      _hb_ot_layout_set_glyph_property (context->layout->face, glyph_id, context->property);

    return true;
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return coverage.sanitize (context, this)
	&& alternateSet.sanitize (context, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  OffsetArrayOf<AlternateSet>
		alternateSet;		/* Array of AlternateSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, alternateSet);
};

struct AlternateSubst
{
  friend struct SubstLookupSubTable;

  private:

  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (context);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (context)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (context);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  AlternateSubstFormat1	format1;
  } u;
};


struct Ligature
{
  friend struct LigatureSet;

  private:
  inline bool apply (hb_apply_context_t *context, bool is_mark) const
  {
    TRACE_APPLY ();
    unsigned int i, j;
    unsigned int count = component.len;
    unsigned int end = MIN (context->buffer->in_length, context->buffer->in_pos + context->context_length);
    if (unlikely (context->buffer->in_pos + count > end))
      return false;

    for (i = 1, j = context->buffer->in_pos + 1; i < count; i++, j++)
    {
      unsigned int property;
      while (_hb_ot_layout_skip_mark (context->layout->face, IN_INFO (j), context->lookup_flag, &property))
      {
	if (unlikely (j + count - i == end))
	  return false;
	j++;
      }

      if (!(property & HB_OT_LAYOUT_GLYPH_CLASS_MARK))
	is_mark = false;

      if (likely (IN_GLYPH (j) != component[i]))
        return false;
    }
    /* This is just a guess ... */
    if (_hb_ot_layout_has_new_glyph_classes (context->layout->face))
      _hb_ot_layout_set_glyph_class (context->layout->face, ligGlyph,
				     is_mark ? HB_OT_LAYOUT_GLYPH_CLASS_MARK
					     : HB_OT_LAYOUT_GLYPH_CLASS_LIGATURE);

    if (j == context->buffer->in_pos + i) /* No input glyphs skipped */
      /* We don't use a new ligature ID if there are no skipped
	 glyphs and the ligature already has an ID. */
      _hb_buffer_add_output_glyphs_be16 (context->buffer, i,
					 1, (const uint16_t *) &ligGlyph,
					 0,
					 IN_LIGID (context->buffer->in_pos) && !IN_COMPONENT (context->buffer->in_pos) ?
					 0xFFFF : _hb_buffer_allocate_lig_id (context->buffer));
    else
    {
      unsigned int lig_id = _hb_buffer_allocate_lig_id (context->buffer);
      _hb_buffer_add_output_glyph (context->buffer, ligGlyph, 0xFFFF, lig_id);

      /* Now we must do a second loop to copy the skipped glyphs to
	 `out' and assign component values to it.  We start with the
	 glyph after the first component.  Glyphs between component
	 i and i+1 belong to component i.  Together with the lig_id
	 value it is later possible to check whether a specific
	 component value really belongs to a given ligature. */

      for ( i = 1; i < count; i++ )
      {
	while (_hb_ot_layout_skip_mark (context->layout->face, IN_CURINFO (), context->lookup_flag, NULL))
	  _hb_buffer_add_output_glyph (context->buffer, IN_CURGLYPH (), i, lig_id);

	(context->buffer->in_pos)++;
      }
    }

    return true;
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return ligGlyph.sanitize (context)
        && component.sanitize (context);
  }

  private:
  GlyphID	ligGlyph;		/* GlyphID of ligature to substitute */
  HeadlessArrayOf<GlyphID>
		component;		/* Array of component GlyphIDs--start
					 * with the second  component--ordered
					 * in writing direction */
  public:
  DEFINE_SIZE_ARRAY (4, component);
};

struct LigatureSet
{
  friend struct LigatureSubstFormat1;

  private:
  inline bool apply (hb_apply_context_t *context, bool is_mark) const
  {
    TRACE_APPLY ();
    unsigned int num_ligs = ligature.len;
    for (unsigned int i = 0; i < num_ligs; i++)
    {
      const Ligature &lig = this+ligature[i];
      if (lig.apply (context, is_mark))
        return true;
    }

    return false;
  }

  public:
  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return ligature.sanitize (context, this);
  }

  private:
  OffsetArrayOf<Ligature>
		ligature;		/* Array LigatureSet tables
					 * ordered by preference */
  public:
  DEFINE_SIZE_ARRAY (2, ligature);
};

struct LigatureSubstFormat1
{
  friend struct LigatureSubst;

  private:
  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    hb_codepoint_t glyph_id = IN_CURGLYPH ();

    bool first_is_mark = !!(context->property & HB_OT_LAYOUT_GLYPH_CLASS_MARK);

    unsigned int index = (this+coverage) (glyph_id);
    if (likely (index == NOT_COVERED))
      return false;

    const LigatureSet &lig_set = this+ligatureSet[index];
    return lig_set.apply (context, first_is_mark);
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    return coverage.sanitize (context, this)
	&& ligatureSet.sanitize (context, this);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of Substitution table */
  OffsetArrayOf<LigatureSet>
		ligatureSet;		/* Array LigatureSet tables
					 * ordered by Coverage Index */
  public:
  DEFINE_SIZE_ARRAY (6, ligatureSet);
};

struct LigatureSubst
{
  friend struct SubstLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (context);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (context)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (context);
    default:return true;
    }
  }

  private:
  union {
  USHORT		format;		/* Format identifier */
  LigatureSubstFormat1	format1;
  } u;
};



static inline bool substitute_lookup (hb_apply_context_t *context, unsigned int lookup_index);

struct ContextSubst : Context
{
  friend struct SubstLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    return Context::apply (context, substitute_lookup);
  }
};

struct ChainContextSubst : ChainContext
{
  friend struct SubstLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    return ChainContext::apply (context, substitute_lookup);
  }
};


struct ExtensionSubst : Extension
{
  friend struct SubstLookupSubTable;
  friend struct SubstLookup;

  private:
  inline const struct SubstLookupSubTable& get_subtable (void) const
  {
    unsigned int offset = get_offset ();
    if (unlikely (!offset)) return Null(SubstLookupSubTable);
    return StructAtOffset<SubstLookupSubTable> (this, offset);
  }

  inline bool apply (hb_apply_context_t *context) const;

  inline bool sanitize (hb_sanitize_context_t *context);

  inline bool is_reverse (void) const;
};


struct ReverseChainSingleSubstFormat1
{
  friend struct ReverseChainSingleSubst;

  private:
  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    if (unlikely (context->context_length != NO_CONTEXT))
      return false; /* No chaining to this type */

    unsigned int index = (this+coverage) (IN_CURGLYPH ());
    if (likely (index == NOT_COVERED))
      return false;

    const OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage> > (backtrack);
    const ArrayOf<GlyphID> &substitute = StructAfter<ArrayOf<GlyphID> > (lookahead);

    if (match_backtrack (context,
			 backtrack.len, (USHORT *) backtrack.array,
			 match_coverage, this) &&
        match_lookahead (context,
			 lookahead.len, (USHORT *) lookahead.array,
			 match_coverage, this,
			 1))
    {
      IN_CURGLYPH () = substitute[index];
      context->buffer->in_pos--; /* Reverse! */
      return true;
    }

    return false;
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (!(coverage.sanitize (context, this)
       && backtrack.sanitize (context, this)))
      return false;
    OffsetArrayOf<Coverage> &lookahead = StructAfter<OffsetArrayOf<Coverage> > (backtrack);
    if (!lookahead.sanitize (context, this))
      return false;
    ArrayOf<GlyphID> &substitute = StructAfter<ArrayOf<GlyphID> > (lookahead);
    return substitute.sanitize (context);
  }

  private:
  USHORT	format;			/* Format identifier--format = 1 */
  OffsetTo<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of table */
  OffsetArrayOf<Coverage>
		backtrack;		/* Array of coverage tables
					 * in backtracking sequence, in  glyph
					 * sequence order */
  OffsetArrayOf<Coverage>
		lookaheadX;		/* Array of coverage tables
					 * in lookahead sequence, in glyph
					 * sequence order */
  ArrayOf<GlyphID>
		substituteX;		/* Array of substitute
					 * GlyphIDs--ordered by Coverage Index */
  public:
  DEFINE_SIZE_MIN (10);
};

struct ReverseChainSingleSubst
{
  friend struct SubstLookupSubTable;

  private:
  inline bool apply (hb_apply_context_t *context) const
  {
    TRACE_APPLY ();
    switch (u.format) {
    case 1: return u.format1.apply (context);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (!u.format.sanitize (context)) return false;
    switch (u.format) {
    case 1: return u.format1.sanitize (context);
    default:return true;
    }
  }

  private:
  union {
  USHORT				format;		/* Format identifier */
  ReverseChainSingleSubstFormat1	format1;
  } u;
};



/*
 * SubstLookup
 */

struct SubstLookupSubTable
{
  friend struct SubstLookup;

  enum {
    Single		= 1,
    Multiple		= 2,
    Alternate		= 3,
    Ligature		= 4,
    Context		= 5,
    ChainContext	= 6,
    Extension		= 7,
    ReverseChainSingle	= 8
  };

  inline bool apply (hb_apply_context_t *context, unsigned int lookup_type) const
  {
    TRACE_APPLY ();
    switch (lookup_type) {
    case Single:		return u.single.apply (context);
    case Multiple:		return u.multiple.apply (context);
    case Alternate:		return u.alternate.apply (context);
    case Ligature:		return u.ligature.apply (context);
    case Context:		return u.context.apply (context);
    case ChainContext:		return u.chainContext.apply (context);
    case Extension:		return u.extension.apply (context);
    case ReverseChainSingle:	return u.reverseChainContextSingle.apply (context);
    default:return false;
    }
  }

  inline bool sanitize (hb_sanitize_context_t *context, unsigned int lookup_type) {
    TRACE_SANITIZE ();
    switch (lookup_type) {
    case Single:		return u.single.sanitize (context);
    case Multiple:		return u.multiple.sanitize (context);
    case Alternate:		return u.alternate.sanitize (context);
    case Ligature:		return u.ligature.sanitize (context);
    case Context:		return u.context.sanitize (context);
    case ChainContext:		return u.chainContext.sanitize (context);
    case Extension:		return u.extension.sanitize (context);
    case ReverseChainSingle:	return u.reverseChainContextSingle.sanitize (context);
    default:return true;
    }
  }

  private:
  union {
  USHORT			sub_format;
  SingleSubst			single;
  MultipleSubst			multiple;
  AlternateSubst		alternate;
  LigatureSubst			ligature;
  ContextSubst			context;
  ChainContextSubst		chainContext;
  ExtensionSubst		extension;
  ReverseChainSingleSubst	reverseChainContextSingle;
  } u;
  public:
  DEFINE_SIZE_UNION (2, sub_format);
};


struct SubstLookup : Lookup
{
  inline const SubstLookupSubTable& get_subtable (unsigned int i) const
  { return this+CastR<OffsetArrayOf<SubstLookupSubTable> > (subTable)[i]; }

  inline static bool lookup_type_is_reverse (unsigned int lookup_type)
  { return lookup_type == SubstLookupSubTable::ReverseChainSingle; }

  inline bool is_reverse (void) const
  {
    unsigned int type = get_type ();
    if (unlikely (type == SubstLookupSubTable::Extension))
      return CastR<ExtensionSubst> (get_subtable(0)).is_reverse ();
    return lookup_type_is_reverse (type);
  }


  inline bool apply_once (hb_ot_layout_context_t *layout,
			  hb_buffer_t *buffer,
			  unsigned int context_length,
			  unsigned int nesting_level_left) const
  {
    unsigned int lookup_type = get_type ();
    hb_apply_context_t context[1] = {{0}};

    context->layout = layout;
    context->buffer = buffer;
    context->context_length = context_length;
    context->nesting_level_left = nesting_level_left;
    context->lookup_flag = get_flag ();

    if (!_hb_ot_layout_check_glyph_property (context->layout->face, IN_CURINFO (), context->lookup_flag, &context->property))
      return false;

    if (unlikely (lookup_type == SubstLookupSubTable::Extension))
    {
      /* The spec says all subtables should have the same type.
       * This is specially important if one has a reverse type!
       *
       * This is rather slow to do this here for every glyph,
       * but it's easiest, and who uses extension lookups anyway?!*/
      unsigned int count = get_subtable_count ();
      unsigned int type = get_subtable(0).u.extension.get_type ();
      for (unsigned int i = 1; i < count; i++)
        if (get_subtable(i).u.extension.get_type () != type)
	  return false;
    }

    unsigned int count = get_subtable_count ();
    for (unsigned int i = 0; i < count; i++)
      if (get_subtable (i).apply (context, lookup_type))
	return true;

    return false;
  }

  inline bool apply_string (hb_ot_layout_context_t *layout,
			    hb_buffer_t *buffer,
			    hb_mask_t    mask) const
  {
#undef BUFFER
#define BUFFER buffer
    bool ret = false;

    if (unlikely (!buffer->in_length))
      return false;

    if (likely (!is_reverse ()))
    {
	/* in/out forward substitution */
	_hb_buffer_clear_output (buffer);
	buffer->in_pos = 0;
	while (buffer->in_pos < buffer->in_length)
	{
	  if ((~IN_MASK (buffer->in_pos) & mask) &&
	      apply_once (layout, buffer, NO_CONTEXT, MAX_NESTING_LEVEL))
	    ret = true;
	  else
	    _hb_buffer_next_glyph (buffer);

	}
	if (ret)
	  _hb_buffer_swap (buffer);
    }
    else
    {
	/* in-place backward substitution */
	buffer->in_pos = buffer->in_length - 1;
	do
	{
	  if ((~IN_MASK (buffer->in_pos) & mask) &&
	      apply_once (layout, buffer, NO_CONTEXT, MAX_NESTING_LEVEL))
	    ret = true;
	  else
	    buffer->in_pos--;

	}
	while ((int) buffer->in_pos >= 0);
    }

    return ret;
  }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (unlikely (!Lookup::sanitize (context))) return false;
    OffsetArrayOf<SubstLookupSubTable> &list = CastR<OffsetArrayOf<SubstLookupSubTable> > (subTable);
    return list.sanitize (context, this, get_type ());
  }
};

typedef OffsetListOf<SubstLookup> SubstLookupList;

/*
 * GSUB
 */

struct GSUB : GSUBGPOS
{
  static const hb_tag_t Tag	= HB_OT_TAG_GSUB;

  inline const SubstLookup& get_lookup (unsigned int i) const
  { return CastR<SubstLookup> (GSUBGPOS::get_lookup (i)); }

  inline bool substitute_lookup (hb_ot_layout_context_t *layout,
				 hb_buffer_t  *buffer,
			         unsigned int  lookup_index,
				 hb_mask_t     mask) const
  { return get_lookup (lookup_index).apply_string (layout, buffer, mask); }

  inline bool sanitize (hb_sanitize_context_t *context) {
    TRACE_SANITIZE ();
    if (unlikely (!GSUBGPOS::sanitize (context))) return false;
    OffsetTo<SubstLookupList> &list = CastR<OffsetTo<SubstLookupList> > (lookupList);
    return list.sanitize (context, this);
  }
  public:
  DEFINE_SIZE_STATIC (10);
};


/* Out-of-class implementation for methods recursing */

inline bool ExtensionSubst::apply (hb_apply_context_t *context) const
{
  TRACE_APPLY ();
  return get_subtable ().apply (context, get_type ());
}

inline bool ExtensionSubst::sanitize (hb_sanitize_context_t *context)
{
  TRACE_SANITIZE ();
  if (unlikely (!Extension::sanitize (context))) return false;
  unsigned int offset = get_offset ();
  if (unlikely (!offset)) return true;
  return StructAtOffset<SubstLookupSubTable> (this, offset).sanitize (context, get_type ());
}

inline bool ExtensionSubst::is_reverse (void) const
{
  unsigned int type = get_type ();
  if (unlikely (type == SubstLookupSubTable::Extension))
    return CastR<ExtensionSubst> (get_subtable()).is_reverse ();
  return SubstLookup::lookup_type_is_reverse (type);
}

static inline bool substitute_lookup (hb_apply_context_t *context, unsigned int lookup_index)
{
  const GSUB &gsub = *(context->layout->face->ot_layout.gsub);
  const SubstLookup &l = gsub.get_lookup (lookup_index);

  if (unlikely (context->nesting_level_left == 0))
    return false;

  if (unlikely (context->context_length < 1))
    return false;

  return l.apply_once (context->layout, context->buffer, context->context_length, context->nesting_level_left - 1);
}


#endif /* HB_OT_LAYOUT_GSUB_PRIVATE_HH */
