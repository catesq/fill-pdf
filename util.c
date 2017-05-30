#include "fill.h"
#include "mupdf/pdf.h"
#include "mupdf/memento.h"

// this file is basically pdf-appearance.c with a few tweaks to customise appearance
// mupdf does not expose much of this behaviour (complex internal api how could they tbh)
// copying huge chunks of their code seems to be the only option to change the signature appearance to how I want

pdf_obj *u_pdf_add_image(fz_context *ctx, pdf_document *doc, fz_image *image, int mask) {
    fz_pixmap *pixmap = NULL;
    pdf_obj *imobj = NULL;
    pdf_obj *imref = NULL;
    fz_compressed_buffer *cbuffer = NULL;
    fz_compression_params *cp = NULL;
    fz_buffer *buffer = NULL;
    fz_colorspace *colorspace = image->colorspace;
    unsigned char digest[16];
    unsigned char *alpha = NULL;
    /* If we can maintain compression, do so */
    cbuffer = fz_compressed_image_buffer(ctx, image);

    fz_var(pixmap);
    fz_var(imobj);
    fz_var(imref);
    fz_var(cbuffer);
    fz_var(cp);
    fz_var(buffer);
    fz_var(colorspace);
    fz_var(digest);
    fz_var(alpha);
    fz_try(ctx)
    {
        /* Before we add this image as a resource check if the same image
         * already exists in our resources for this doc.  If yes, then
         * hand back that reference */
        imref = u_pdf_find_image_resource(ctx, doc, image, digest);
        if (imref == NULL) {
            if (cbuffer != NULL && cbuffer->params.type != FZ_IMAGE_PNG && cbuffer->params.type != FZ_IMAGE_TIFF) {
                buffer = fz_keep_buffer(ctx, cbuffer->buffer);
                cp = &cbuffer->params;
            } else {
                unsigned int size;
                int n;
                unsigned char *d;
                unsigned char *ap;

                /* Currently, set to maintain resolution; should we consider
                 * subsampling here according to desired output res? */
                pixmap = fz_get_pixmap_from_image(ctx, image, NULL, NULL, NULL, NULL);
                colorspace = pixmap->colorspace; /* May be different to image->colorspace! */
                n = (pixmap->n == 1 ? 1 : pixmap->n - 1);
                size = image->w * image->h * n;
                d = fz_malloc(ctx, size);
                buffer = fz_new_buffer_from_data(ctx, d, size);

                if (pixmap->n == 1) {
                    memcpy(d, pixmap->samples, size);
                } else {
                    /* Need to remove the alpha plane */
                    ap = alpha = fz_malloc(ctx, image->w * image->h);
                    unsigned char *s = pixmap->samples;
                    int mod = n;
                    while (size--)
                    {
                        *d++ = *s++;
                        mod--;
                        if (mod == 0) {
                            *ap++ = *s++;
                            mod = n;
                        }
                    }

                    if(!image->mask) {
                        fz_pixmap *mask_pixmap = fz_new_pixmap_from_8bpp_data(ctx, 0, 0, image->w, image->h, alpha, image->w);
                        fz_image *mask_img = fz_new_image_from_pixmap(ctx, mask_pixmap, NULL);
                        image->mask = mask_img;
                    }

                    fz_free(ctx, alpha);
                }
            }

            imobj = pdf_new_dict(ctx, doc, 3);
            pdf_dict_put_drop(ctx, imobj, PDF_NAME_Type, PDF_NAME_XObject);
            pdf_dict_put_drop(ctx, imobj, PDF_NAME_Subtype, PDF_NAME_Image);
            pdf_dict_put_drop(ctx, imobj, PDF_NAME_Width, pdf_new_int(ctx, doc, image->w));
            pdf_dict_put_drop(ctx, imobj, PDF_NAME_Height, pdf_new_int(ctx, doc, image->h));
            int colorspace_n = fz_colorspace_n(ctx, colorspace);

            if (mask) {
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_ImageMask, pdf_new_bool(ctx, doc, 1));
            } else {
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_BitsPerComponent, pdf_new_int(ctx, doc, image->bpc));
                if (!colorspace || colorspace_n == 1)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_ColorSpace, PDF_NAME_DeviceGray);
                else if (colorspace_n == 3)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_ColorSpace, PDF_NAME_DeviceRGB);
                else if (colorspace_n == 4)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_ColorSpace, PDF_NAME_DeviceCMYK);
            }

            switch (cp ? cp->type : FZ_IMAGE_UNKNOWN)
            {
            case FZ_IMAGE_UNKNOWN: /* Unknown also means raw */
            default:
                break;
            case FZ_IMAGE_JPEG:
                if (cp->u.jpeg.color_transform != -1)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_ColorTransform, pdf_new_int(ctx, doc, cp->u.jpeg.color_transform));
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_Filter, PDF_NAME_DCTDecode);
                break;
            case FZ_IMAGE_JPX:
                if (cp->u.jpx.smask_in_data)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_SMaskInData, pdf_new_int(ctx, doc, cp->u.jpx.smask_in_data));
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_Filter, PDF_NAME_JPXDecode);
                break;
            case FZ_IMAGE_FAX:
                if (cp->u.fax.columns)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Columns, pdf_new_int(ctx, doc, cp->u.fax.columns));
                if (cp->u.fax.rows)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Rows, pdf_new_int(ctx, doc, cp->u.fax.rows));
                if (cp->u.fax.k)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_K, pdf_new_int(ctx, doc, cp->u.fax.k));
                if (cp->u.fax.end_of_line)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_EndOfLine, pdf_new_int(ctx, doc, cp->u.fax.end_of_line));
                if (cp->u.fax.encoded_byte_align)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_EncodedByteAlign, pdf_new_int(ctx, doc, cp->u.fax.encoded_byte_align));
                if (cp->u.fax.end_of_block)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_EndOfBlock, pdf_new_int(ctx, doc, cp->u.fax.end_of_block));
                if (cp->u.fax.black_is_1)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_BlackIs1, pdf_new_int(ctx, doc, cp->u.fax.black_is_1));
                if (cp->u.fax.damaged_rows_before_error)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_DamagedRowsBeforeError, pdf_new_int(ctx, doc, cp->u.fax.damaged_rows_before_error));
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_Filter, PDF_NAME_CCITTFaxDecode);
                break;
//            case FZ_IMAGE_JBIG2:
                /* FIXME - jbig2globals */
//                cp->type = FZ_IMAGE_UNKNOWN;
//                break;
            case FZ_IMAGE_FLATE:
                if (cp->u.flate.columns)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Columns, pdf_new_int(ctx, doc, cp->u.flate.columns));
                if (cp->u.flate.colors)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Colors, pdf_new_int(ctx, doc, cp->u.flate.colors));
                if (cp->u.flate.predictor)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Predictor, pdf_new_int(ctx, doc, cp->u.flate.predictor));
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_Filter, PDF_NAME_FlateDecode);
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_BitsPerComponent, pdf_new_int(ctx, doc, image->bpc));
                break;
            case FZ_IMAGE_LZW:
                if (cp->u.lzw.columns)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Columns, pdf_new_int(ctx, doc, cp->u.lzw.columns));
                if (cp->u.lzw.colors)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Colors, pdf_new_int(ctx, doc, cp->u.lzw.colors));
                if (cp->u.lzw.predictor)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_Predictor, pdf_new_int(ctx, doc, cp->u.lzw.predictor));
                if (cp->u.lzw.early_change)
                    pdf_dict_put_drop(ctx, imobj, PDF_NAME_EarlyChange, pdf_new_int(ctx, doc, cp->u.lzw.early_change));
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_Filter, PDF_NAME_LZWDecode);
                break;
            case FZ_IMAGE_RLD:
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_Filter, PDF_NAME_RunLengthDecode);
                break;
            }

            if (image->mask)
                pdf_dict_put_drop(ctx, imobj, PDF_NAME_SMask, pdf_add_image(ctx, doc, image->mask, 0));

            imref = pdf_add_object(ctx, doc, imobj);
            pdf_update_stream(ctx, doc, imref, buffer, 1);

            /* Add ref to our image resource hash table. */
            imref = pdf_insert_image_resource(ctx, doc, digest, imref);

        }
    }
    fz_always(ctx)
    {
        fz_drop_buffer(ctx, buffer);
        pdf_drop_obj(ctx, imobj);
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx)
    {
        pdf_drop_obj(ctx, imref);
        fz_rethrow(ctx);
    }
    return imref;
}



pdf_obj *u_pdf_find_image_resource(fz_context *ctx, pdf_document *doc, fz_image *item, unsigned char digest[16]) {
    pdf_obj *res;

    if (!doc->resources.images) {
        doc->resources.images = fz_new_hash_table(ctx, 4096, 16, -1);
        u_pdf_preload_image_resources(ctx, doc);
    }

    /* Create md5 and see if we have the item in our table */
    u_fz_md5_image(ctx, item, digest);
    res = fz_hash_find(ctx, doc->resources.images, digest);
    if (res)
        pdf_keep_obj(ctx, res);
    return res;
}

void u_pdf_preload_image_resources(fz_context *ctx, pdf_document *doc) {
    int len, k;
    pdf_obj *obj;
    pdf_obj *type;
    pdf_obj *res = NULL;
    fz_image *image = NULL;
    unsigned char digest[16];

    fz_var(len);
    fz_var(k);
    fz_var(obj);
    fz_var(type);
    fz_var(res);
    fz_var(image);
    fz_var(digest);
    fz_try(ctx)
    {
        len = pdf_count_objects(ctx, doc);

        for (k = 1; k < len; k++) {
            obj = pdf_new_indirect(ctx, doc, k, 0);

            type = pdf_dict_get(ctx, obj, PDF_NAME_Subtype);
            if (pdf_name_eq(ctx, type, PDF_NAME_Image))
            {
                image = pdf_load_image(ctx, doc, obj);
                u_fz_md5_image(ctx, image, digest);
                fz_drop_image(ctx, image);
                image = NULL;

                /* Do not allow overwrites. */
                if (!fz_hash_find(ctx, doc->resources.images, digest))
                    fz_hash_insert(ctx, doc->resources.images, digest, pdf_keep_obj(ctx, obj));
            }
            pdf_drop_obj(ctx, obj);
            obj = NULL;
        }
    }
    fz_always(ctx)
    {
        fz_drop_image(ctx, image);
        pdf_drop_obj(ctx, obj);
    }
    fz_catch(ctx)
    {
        fz_rethrow(ctx);
    }
}


void u_fz_md5_image(fz_context *ctx, fz_image *image, unsigned char digest[16]) {
    fz_pixmap *pixmap;
    fz_md5 state;
    int h;
    unsigned char *d;

    pixmap = fz_get_pixmap_from_image(ctx, image, NULL, NULL, 0, 0);
    fz_md5_init(&state);
    d = pixmap->samples;
    h = pixmap->h;
    while (h--)
    {
        fz_md5_update(&state, d, pixmap->w * pixmap->n);
        d += pixmap->stride;
    }
    fz_md5_final(&state, digest);
    fz_drop_pixmap(ctx, pixmap);
}


void u_pdf_sign_signature(fz_context *ctx, pdf_document *doc, pdf_widget *widget, const char *sigfile, const char *password, vg_pathlist *pathlist, const char *overlay_msg) {
    pdf_signer *signer = pdf_read_pfx(ctx, sigfile, password);
    pdf_designated_name *dn = NULL;
    fz_buffer *fzbuf = NULL;

    fz_var(signer);
    fz_var(dn);
    fz_var(fzbuf);
    fz_try(ctx)
    {

        pdf_obj *wobj = ((pdf_annot *)widget)->obj;
        fz_rect rect = fz_empty_rect;

        pdf_signature_set_value(ctx, doc, wobj, signer);

        pdf_to_rect(ctx, pdf_dict_get(ctx, wobj, PDF_NAME_Rect), &rect);
        /* Create an appearance stream only if the signature is intended to be visible */
        if (!fz_is_empty_rect(&rect)) {
            if(overlay_msg == NULL) {
                dn = pdf_signer_designated_name(ctx, signer);
                fzbuf = fz_new_buffer(ctx, 256);
                if (!dn->cn)
                    fz_throw(ctx, FZ_ERROR_GENERIC, "Certificate has no common name");

                fz_buffer_printf(ctx, fzbuf, "cn=%s", dn->cn);

                if (dn->o)
                    fz_buffer_printf(ctx, fzbuf, ", o=%s", dn->o);

                if (dn->ou)
                    fz_buffer_printf(ctx, fzbuf, ", ou=%s", dn->ou);

                if (dn->email)
                    fz_buffer_printf(ctx, fzbuf, ", email=%s", dn->email);

                if (dn->c)
                    fz_buffer_printf(ctx, fzbuf, ", c=%s", dn->c);

                overlay_msg = (char *) fz_string_from_buffer(ctx, fzbuf);
            }

            u_pdf_set_signature_appearance(ctx, doc, (pdf_annot *)widget, pathlist, overlay_msg);
        }
    } fz_always(ctx) {
        pdf_drop_signer(ctx, signer);
        if(dn != NULL) pdf_drop_designated_name(ctx, dn);
        if(fzbuf != NULL) fz_drop_buffer(ctx, fzbuf);
    } fz_catch(ctx) {
        fz_rethrow(ctx);
    }
}

typedef struct font_info_s
{
    pdf_da_info da_rec;
    pdf_font_desc *font;
    float lineheight;
} font_info;


static void add_text(fz_context *ctx, font_info *font_rec, fz_text *text, const char *str, size_t str_len, const fz_matrix *tm_) {
    fz_font *font = font_rec->font->font;
    fz_matrix tm = *tm_;
    int ucs, gid, n;

    while (str_len > 0)
    {
        n = fz_chartorune(&ucs, str);
        str += n;
        str_len -= n;
        gid = fz_encode_character(ctx, font, ucs);
        fz_show_glyph(ctx, text, font, &tm, gid, ucs, 0, 0, FZ_BIDI_NEUTRAL, FZ_LANG_UNSET);
        tm.e += fz_advance_glyph(ctx, font, gid, 0) * font_rec->da_rec.font_size;
    }
}

typedef struct text_splitter_s
{
    font_info *info;
    float width;
    float height;
    float scale;
    float unscaled_width;
    float fontsize;
    float lineheight;
    const char *text;
    int done;
    float x_orig;
    float y_orig;
    float x;
    float x_end;
    size_t text_start;
    size_t text_end;
    int max_lines;
    int retry;
} text_splitter;

static void text_splitter_init(text_splitter *splitter, font_info *info, const char *text, float width, float height, int variable)
{
    float fontsize = info->da_rec.font_size;

    memset(splitter, 0, sizeof(*splitter));
    splitter->info = info;
    splitter->text = text;
    splitter->width = width;
    splitter->unscaled_width = width;
    splitter->height = height;
    splitter->fontsize = fontsize;
    splitter->scale = 1.0;
    splitter->lineheight = fontsize * info->lineheight ;
    /* RJW: The cast in the following line is important, as otherwise
     * under MSVC in the variable = 0 case, splitter->max_lines becomes
     * INT_MIN. */
    splitter->max_lines = variable ? (int)(height/splitter->lineheight) : INT_MAX;
}

static void text_splitter_start_pass(text_splitter *splitter)
{
    splitter->text_end = 0;
    splitter->x_orig = 0;
    splitter->y_orig = 0;
}

static void text_splitter_start_line(text_splitter *splitter)
{
    splitter->x_end = 0;
}

static int text_splitter_layout(fz_context *ctx, text_splitter *splitter)
{
    const char *text;
    float room;
    float stride;
    size_t count;
    size_t len;
    float fontsize = splitter->info->da_rec.font_size;

    splitter->x = splitter->x_end;
    splitter->text_start = splitter->text_end;

    text = splitter->text + splitter->text_start;
    room = splitter->unscaled_width - splitter->x;

    if (strchr("\r\n", text[0]))
    {
        /* Consume return chars and report end of line */
        splitter->text_end += strspn(text, "\r\n");
        splitter->text_start = splitter->text_end;
        splitter->done = (splitter->text[splitter->text_end] == '\0');
        return 0;
    }
    else if (text[0] == ' ')
    {
        /* Treat each space as a word */
        len = 1;
    }
    else
    {
        len = 0;
        while (text[len] != '\0' && !strchr(" \r\n", text[len]))
            len ++;
    }

    stride = pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, len, room, &count);

    /* If not a single char fits although the line is empty, then force one char */
    if (count == 0 && splitter->x == 0.0)
        stride = pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, 1, FLT_MAX, &count);

    if (count < len && splitter->retry)
    {
        /* The word didn't fit and we are in retry mode. Work out the
         * least additional scaling that may help */
        float fitwidth; /* width if we force the word in */
        float hstretchwidth; /* width if we just bump by 10% */
        float vstretchwidth; /* width resulting from forcing in another line */
        float bestwidth;

        fitwidth = splitter->x +
            pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, len, FLT_MAX, &count);
        /* FIXME: temporary fiddle factor. Would be better to work in integers */
        fitwidth *= 1.001f;

        /* Stretching by 10% is worth trying only if processing the first word on the line */
        hstretchwidth = splitter->x == 0.0
            ? splitter->width * 1.1 / splitter->scale
            : FLT_MAX;

        vstretchwidth = splitter->width * (splitter->max_lines + 1) * splitter->lineheight
            / splitter->height;

        bestwidth = fz_min(fitwidth, fz_min(hstretchwidth, vstretchwidth));

        if (bestwidth == vstretchwidth)
            splitter->max_lines ++;

        splitter->scale = splitter->width / bestwidth;
        splitter->unscaled_width = bestwidth;

        splitter->retry = 0;

        /* Try again */
        room = splitter->unscaled_width - splitter->x;
        stride = pdf_text_stride(ctx, splitter->info->font, fontsize, (unsigned char *)text, len, room, &count);
    }

    /* This is not the first word on the line. Best to give up on this line and push
     * the word onto the next */
    if (count < len && splitter->x > 0.0)
        return 0;

    splitter->text_end = splitter->text_start + count;
    splitter->x_end = splitter->x + stride;
    splitter->done = (splitter->text[splitter->text_end] == '\0');
    return 1;
}

static void text_splitter_move(text_splitter *splitter, float newy, float *relx, float *rely)
{
    *relx = splitter->x - splitter->x_orig;
    *rely = newy * splitter->lineheight - splitter->y_orig;

    splitter->x_orig = splitter->x;
    splitter->y_orig = newy * splitter->lineheight;
}

static void text_splitter_retry(text_splitter *splitter)
{
    if (splitter->retry)
    {
        /* Already tried expanding lines. Overflow must
         * be caused by carriage control */
        splitter->max_lines ++;
        splitter->retry = 0;
        splitter->unscaled_width = splitter->width * splitter->max_lines * splitter->lineheight
            / splitter->height;
        splitter->scale = splitter->width / splitter->unscaled_width;
    }
    else
    {
        splitter->retry = 1;
    }
}

static fz_text *fit_text(fz_context *ctx, font_info *font_rec, const char *str, fz_rect *bounds)
{
    float width = bounds->x1 - bounds->x0;
    float height = bounds->y1 - bounds->y0;
    fz_matrix tm;
    fz_text *text = NULL;
    fz_text_span *span;
    text_splitter splitter;
    float ascender;

    /* Initially aim for one-line of text */
    font_rec->da_rec.font_size = height / font_rec->lineheight;

    text_splitter_init(&splitter, font_rec, str, width, height, 1);

    fz_var(text);
    fz_try(ctx)
    {
        int i;
        while (!splitter.done)
        {
            /* Try a layout pass */
            int line = 0;
            float font_size;

            fz_drop_text(ctx, text);
            text = NULL;
            font_size = font_rec->da_rec.font_size;
            fz_scale(&tm, font_size, font_size);
            tm.e = 0;
            tm.f = 0;

            text = fz_new_text(ctx);

            text_splitter_start_pass(&splitter);

            /* Layout unscaled text to a scaled-up width, so that
            * the scaled-down text will fit the unscaled width */

            while (!splitter.done && line < splitter.max_lines)
            {
                /* Layout a line */
                text_splitter_start_line(&splitter);

                while (!splitter.done && text_splitter_layout(ctx, &splitter))
                {
                    if (splitter.text[splitter.text_start] != ' ')
                    {
                        float dx, dy;
                        const char *word = str+splitter.text_start;
                        size_t wordlen = splitter.text_end-splitter.text_start;

                        text_splitter_move(&splitter, -line, &dx, &dy);
                        tm.e += dx;
                        tm.f += dy;
                        add_text(ctx, font_rec, text, word, wordlen, &tm);
                    }
                }

                line ++;
            }

            if (!splitter.done)
                text_splitter_retry(&splitter);
        }

        /* Post process text with the scale determined by the splitter
         * and with the required offset */
        for (span = text->head; span; span = span->next)
        {
            fz_pre_scale(&span->trm, splitter.scale, splitter.scale);
            ascender = font_rec->font->ascent * font_rec->da_rec.font_size * splitter.scale / 1000.0f;
            for (i = 0; i < span->len; i++)
            {
                span->items[i].x = span->items[i].x * splitter.scale + bounds->x0;
                span->items[i].y = span->items[i].y * splitter.scale + bounds->y1 - ascender;
            }
        }
    }
    fz_catch(ctx)
    {
        fz_drop_text(ctx, text);
        fz_rethrow(ctx);
    }

    return text;
}


static void rect_center(const fz_rect *rect, fz_point *c) {
    c->x = (rect->x0 + rect->x1) / 2.0f;
    c->y = (rect->y0 + rect->y1) / 2.0f;
}

static void center_rect_within_rect(const fz_rect *tofit, const fz_rect *within, fz_matrix *mat) {
    float xscale = (within->x1 - within->x0) / (tofit->x1 - tofit->x0);
    float yscale = (within->y1 - within->y0) / (tofit->y1 - tofit->y0);
    float scale = fz_min(xscale, yscale);
    fz_point tofit_center;
    fz_point within_center;

    rect_center(within, &within_center);
    rect_center(tofit, &tofit_center);

    /* Translate "tofit" to be centered on the origin
     * Scale "tofit" to a size that fits within "within"
     * Translate "tofit" to "within's" center
     * Do all the above in reverse order so that we can use the fz_pre_xx functions */
    fz_translate(mat, within_center.x, within_center.y);
    fz_pre_scale(mat, scale, scale);
    fz_pre_translate(mat, -tofit_center.x, -tofit_center.y);
}

static float logo_color[4] = {(float)0x25/(float)0xFF, (float)0x72/(float)0xFF, (float)0xAC/(float)0xFF, 1};

static vg_pathlist *get_default_sig_pathlist() {
    vg_pathlist *plist = vg_new_pathlist();
    vg_path *path = vg_add_path(plist, vg_new_path(VG_FILL, logo_color));
    vg_add_cmd(path, vg_moveto(1, 122.25f, 0.0f));
    vg_add_cmd(path, vg_lineto(1, 122.25f, -14.249f));
    vg_add_cmd(path, vg_curveto(1, 125.98f, -13.842f, 129.73f, -13.518f, 133.5f, -13.277f));
    vg_add_cmd(path, vg_lineto(1, 133.5f, 0.0f));
    vg_add_cmd(path, vg_lineto(1, 122.25f, 0.0f));
    vg_add_cmd(path, vg_close());
    vg_add_cmd(path, vg_moveto(1, 140.251f, 0.0f));
    vg_add_cmd(path, vg_lineto(1, 140.251f, -12.935f));
    vg_add_cmd(path, vg_curveto(1, 152.534f, -12.477f, 165.03f, -12.899f, 177.75f, -14.249f));

    vg_add_cmd(path, vg_lineto(1, 177.75f, -21.749f));
    vg_add_cmd(path, vg_curveto(1, 165.304f, -20.413f, 152.809f, -19.871f, 140.251f, -20.348f));
    vg_add_cmd(path, vg_lineto(1, 140.251f, -39.0f));
    vg_add_cmd(path, vg_lineto(1, 133.5f, -39.0f));
    vg_add_cmd(path, vg_lineto(1, 133.5f, -20.704f));
    vg_add_cmd(path, vg_curveto(1, 129.756f, -20.956f, 126.006f, -21.302f, 122.25f, -21.749f));
    vg_add_cmd(path, vg_lineto(1, 122.25f, -50.999f));
    vg_add_cmd(path, vg_lineto(1, 177.751f, -50.999f));
    vg_add_cmd(path, vg_lineto(1, 177.751f, 0.0f));
    vg_add_cmd(path, vg_lineto(1, 140.251f, 0.0f));
    vg_add_cmd(path, vg_close());

    vg_add_cmd(path, vg_moveto(1, 23.482f, -129.419f));
    vg_add_cmd(path, vg_curveto(1, -20.999f, -199.258f, -0.418f, -292.039f, 69.42f, -336.519f));
    vg_add_cmd(path, vg_curveto(1, 139.259f, -381.0f, 232.04f, -360.419f, 276.52f, -290.581f));
    vg_add_cmd(path, vg_curveto(1, 321.001f, -220.742f, 300.42f, -127.961f, 230.582f, -83.481f));
    vg_add_cmd(path, vg_curveto(1, 160.743f, -39.0f, 67.962f, -59.581f, 23.482f, -129.419f));
    vg_add_cmd(path, vg_close());

    vg_add_cmd(path, vg_moveto(1, 254.751f, -128.492f));
    vg_add_cmd(path, vg_curveto(1, 303.074f, -182.82f, 295.364f, -263.762f, 237.541f, -309.165f));
    vg_add_cmd(path, vg_curveto(1, 179.718f, -354.568f, 93.57f, -347.324f, 45.247f, -292.996f));
    vg_add_cmd(path, vg_curveto(1, -3.076f, -238.668f, 4.634f, -157.726f, 62.457f, -112.323f));
    vg_add_cmd(path, vg_curveto(1, 120.28f, -66.92f, 206.428f, -74.164f, 254.751f, -128.492f));
    vg_add_cmd(path, vg_close());

    vg_add_cmd(path, vg_moveto(1, 111.0f, -98.999f));
    vg_add_cmd(path, vg_curveto(1, 87.424f, -106.253f, 68.25f, -122.249f, 51.75f, -144.749f));
    vg_add_cmd(path, vg_lineto(1, 103.5f, -297.749f));
    vg_add_cmd(path, vg_lineto(1, 213.75f, -298.499f));

    vg_add_cmd(path, vg_curveto(1, 206.25f, -306.749f, 195.744f, -311.478f, 185.25f, -314.249f));
    vg_add_cmd(path, vg_curveto(1, 164.22f, -319.802f, 141.22f, -319.775f, 120.0f, -314.999f));
    vg_add_cmd(path, vg_curveto(1, 96.658f, -309.745f, 77.25f, -298.499f, 55.5f, -283.499f));
    vg_add_cmd(path, vg_curveto(1, 69.75f, -299.249f, 84.617f, -311.546f, 102.75f, -319.499f));
    vg_add_cmd(path, vg_curveto(1, 117.166f, -325.822f, 133.509f, -327.689f, 149.25f, -327.749f));
    vg_add_cmd(path, vg_curveto(1, 164.21f, -327.806f, 179.924f, -326.532f, 193.5f, -320.249f));
    vg_add_cmd(path, vg_curveto(1, 213.95f, -310.785f, 232.5f, -294.749f, 245.25f, -276.749f));

    vg_add_cmd(path, vg_lineto(1, 227.25f, -276.749f));
    vg_add_cmd(path, vg_curveto(1, 213.963f, -276.749f, 197.25f, -263.786f, 197.25f, -250.499f));

    vg_add_cmd(path, vg_lineto(1, 197.25f, -112.499f));
    vg_add_cmd(path, vg_curveto(1, 213.75f, -114.749f, 228.0f, -127.499f, 241.5f, -140.999f));
    vg_add_cmd(path, vg_curveto(1, 231.75f, -121.499f, 215.175f, -109.723f, 197.25f, -101.249f));
    vg_add_cmd(path, vg_curveto(1, 181.5f, -95.249f, 168.412f, -94.775f, 153.0f, -94.499f));
    vg_add_cmd(path, vg_curveto(1, 139.42f, -94.256f, 120.75f, -95.999f, 111.0f, -98.999f));
    vg_add_cmd(path, vg_close());

    vg_add_cmd(path, vg_moveto(1, 125.25f, -105.749f));
    vg_add_cmd(path, vg_lineto(1, 125.25f, -202.499f));
    vg_add_cmd(path, vg_lineto(1, 95.25f, -117.749f));
    vg_add_cmd(path, vg_curveto(1, 105.75f, -108.749f, 114.0f, -105.749f, 125.25f, -105.749f));
    vg_add_cmd(path, vg_close());

    return plist;
}


static void font_info_fin(fz_context *ctx, font_info *font_rec)
{
    pdf_drop_font(ctx, font_rec->font);
    font_rec->font = NULL;
    pdf_da_info_fin(ctx, &font_rec->da_rec);
}

static void get_font_info(fz_context *ctx, pdf_document *doc, pdf_obj *dr, char *da, font_info *font_rec) {
    pdf_font_desc *font;

    pdf_parse_da(ctx, da, &font_rec->da_rec);
    if (font_rec->da_rec.font_name == NULL)
        fz_throw(ctx, FZ_ERROR_GENERIC, "No font name in default appearance");

    font_rec->font = font = pdf_load_font(ctx, doc, dr, pdf_dict_gets(ctx, pdf_dict_get(ctx, dr, PDF_NAME_Font), font_rec->da_rec.font_name), 0);
    font_rec->lineheight = 1.0;
    if (font && font->ascent != 0.0f && font->descent != 0.0f)
        font_rec->lineheight = (font->ascent - font->descent) / 1000.0;

}


static void u_insert_signature_appearance_layers(fz_context *ctx, pdf_document *doc, pdf_annot *annot)
{
    pdf_obj *ap = pdf_dict_getl(ctx, annot->obj, PDF_NAME_AP, PDF_NAME_N, NULL);
    pdf_obj *main_ap = NULL;
    pdf_obj *frm = NULL;
    pdf_obj *n0 = NULL;
    fz_rect bbox;
    fz_buffer *fzbuf = NULL;

    pdf_to_rect(ctx, pdf_dict_get(ctx, ap, PDF_NAME_BBox), &bbox);

    fz_var(ap);
    fz_var(main_ap);
    fz_var(frm);
    fz_var(bbox);

    fz_var(n0);
    fz_var(fzbuf);
    fz_try(ctx)
    {
        main_ap = pdf_new_xobject(ctx, doc, &bbox, &fz_identity);
        frm = pdf_new_xobject(ctx, doc, &bbox, &fz_identity);
        n0 = pdf_new_xobject(ctx, doc, &bbox, &fz_identity);

        pdf_dict_putl(ctx, main_ap, frm, PDF_NAME_Resources, PDF_NAME_XObject, PDF_NAME_FRM, NULL);
        fzbuf = fz_new_buffer(ctx, 8);
        fz_buffer_printf(ctx, fzbuf, "/FRM Do");
        pdf_update_stream(ctx, doc, main_ap, fzbuf, 0);
        fz_drop_buffer(ctx, fzbuf);
        fzbuf = NULL;

        pdf_dict_putl(ctx, frm, n0, PDF_NAME_Resources, PDF_NAME_XObject, PDF_NAME_n0, NULL);
        pdf_dict_putl(ctx, frm, ap, PDF_NAME_Resources, PDF_NAME_XObject, PDF_NAME_n2, NULL);
        fzbuf = fz_new_buffer(ctx, 8);
        fz_buffer_printf(ctx, fzbuf, "q 1 0 0 1 0 0 cm /n0 Do Q q 1 0 0 1 0 0 cm /n2 Do Q");
        pdf_update_stream(ctx, doc, frm, fzbuf, 0);
        fz_drop_buffer(ctx, fzbuf);
        fzbuf = NULL;

        fzbuf = fz_new_buffer(ctx, 8);
        fz_buffer_printf(ctx, fzbuf, "%% DSBlank");
        pdf_update_stream(ctx, doc, n0, fzbuf, 0);
        fz_drop_buffer(ctx, fzbuf);
        fzbuf = NULL;

        pdf_dict_putl(ctx, annot->obj, main_ap, PDF_NAME_AP, PDF_NAME_N, NULL);
    }
    fz_always(ctx)
    {
        pdf_drop_obj(ctx, main_ap);
        pdf_drop_obj(ctx, frm);
        pdf_drop_obj(ctx, n0);
    }
    fz_catch(ctx)
    {
        fz_drop_buffer(ctx, fzbuf);
        fz_rethrow(ctx);
    }
}

void u_pdf_set_signature_appearance(fz_context *ctx, pdf_document *doc, pdf_annot *annot, vg_pathlist *pathlist, const char *msg_1) {
    pdf_obj *obj = annot->obj;
    pdf_obj *dr = pdf_dict_getl(ctx, pdf_trailer(ctx, doc), PDF_NAME_Root, PDF_NAME_AcroForm, PDF_NAME_DR, NULL);
    fz_display_list *dlist = NULL;
    fz_device *dev = NULL;
    font_info font_rec;
    fz_text *text = NULL;
    fz_colorspace *cs = NULL;
    fz_buffer *fzbuf = NULL;
    fz_matrix page_ctm;

    if(pathlist == NULL) {
        pathlist = get_default_sig_pathlist();
    }

    pdf_page_transform(ctx, annot->page, NULL, &page_ctm);

    if (!dr)
        pdf_dict_putl_drop(ctx, pdf_trailer(ctx, doc), pdf_new_dict(ctx, doc, 1), PDF_NAME_Root, PDF_NAME_AcroForm, PDF_NAME_DR, NULL);

    memset(&font_rec, 0, sizeof(font_rec));

    fz_var(obj);
    fz_var(dr);
    fz_var(dlist);
    fz_var(dev);
    fz_var(font_rec);
    fz_var(text);
    fz_var(cs);
    fz_var(fzbuf);
    fz_var(page_ctm);
    fz_var(pathlist);
    fz_try(ctx)
    {
        char *da = pdf_to_str_buf(ctx, pdf_dict_get(ctx, obj, PDF_NAME_DA));
        fz_rect annot_rect;
        fz_rect rect;

        pdf_to_rect(ctx, pdf_dict_get(ctx, annot->obj, PDF_NAME_Rect), &annot_rect);
        rect = annot_rect;

        dlist = fz_new_display_list(ctx, NULL);
        dev = fz_new_list_device(ctx, dlist);

        vg_draw_pathlist(ctx, dev, &rect, &page_ctm, pathlist);

        rect = annot_rect;
        get_font_info(ctx, doc, dr, da, &font_rec);

        switch (font_rec.da_rec.col_size)
        {
        case 1: cs = fz_device_gray(ctx); break;
        case 3: cs = fz_device_rgb(ctx); break;
        case 4: cs = fz_device_cmyk(ctx); break;
        }

        /* Display the name in the left-hand half of the form field */
//        rect.x1 = (rect.x0 + rect.x1)/2.0f;

        rect.x0 += 1;
        rect.x1 -= 1;
        text = fit_text(ctx, &font_rec, msg_1, &rect);
        fz_fill_text(ctx, dev, text, &page_ctm, cs, font_rec.da_rec.col, 1.0f);
        fz_drop_text(ctx, text);
        text = NULL;

        /* Display the distinguished name in the right-hand half */
//        fzbuf = fz_new_buffer(ctx, 256);
//        fz_buffer_printf(ctx, fzbuf, "Digitally signed by %s", name);
//        fz_buffer_printf(ctx, fzbuf, "%s", msg_2);
//        rect = annot_rect;
//        rect.x0 = (rect.x0 + rect.x1)/2.0f;
//        text = fit_text(ctx, &font_rec, fz_string_from_buffer(ctx, fzbuf), &rect);
//        fz_fill_text(ctx, dev, text, &page_ctm, cs, font_rec.da_rec.col, 1.0f);

        fz_close_device(ctx, dev);
        rect = annot_rect;
        fz_transform_rect(&rect, &page_ctm);
        pdf_set_annot_appearance(ctx, doc, annot, &rect, dlist);


        /* Drop the cached xobject from the annotation structure to
         * force a redraw on next pdf_update_page call */
        pdf_drop_xobject(ctx, annot->ap);
        annot->ap = NULL;

        u_insert_signature_appearance_layers(ctx, doc, annot);
    }
    fz_always(ctx)
    {
//
        fz_drop_device(ctx, dev);
        fz_drop_display_list(ctx, dlist);
        font_info_fin(ctx, &font_rec);
        fz_drop_text(ctx, text);
        fz_drop_colorspace(ctx, cs);
        fz_drop_buffer(ctx, fzbuf);
        vg_free_pathlist(pathlist);
    }
    fz_catch(ctx)
    {
        fz_rethrow(ctx);
    }
}
