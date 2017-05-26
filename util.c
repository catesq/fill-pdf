#include "fill.h"
#include "mupdf/pdf.h"


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
    fz_var(buffer);
    fz_var(imobj);
    fz_var(imref);
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

    fz_var(obj);
    fz_var(image);
    fz_var(res);
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
