#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "fill.h"
#include "zlib.h"

void cmplt_fill_all(pdf_env *env) {
    json_error_t json_err;

    FILE *data_file;

    if(env->fill.dataFile)
        data_file = fopen(env->fill.dataFile, "r");
    else
        data_file = stdin;

    json_t *data_json = json_loadf(data_file, 0, &json_err);

    if (data_json == NULL)
        return;

    if (!json_is_object(data_json)) {
        fprintf(stderr, "input data invalid. json root must be an object");
        goto data_exit;
    }

    env->fill.json_input_data = data_json;

    json_t *template = json_load_file(env->fill.tplFile, 0, &json_err);

    if (template == NULL) {
        fprintf(stderr, "Unable to load template file '%s'", env->fill.tplFile);
        goto tpl_exit;
    }

    if(!json_is_object(template)) {
        fprintf(stderr, "Invalid template file '%s'. json root must be an object.", env->fill.tplFile);
        goto tpl_exit;
    }

    const char* obj_idx;
    int page_idx, item_idx;
    json_t *page_val, *item_val;

    int updated_doc = 0;
    json_object_foreach(template, obj_idx, page_val) {
        // filter pages in the tpl.json.
        if(!str_is_all_digits(obj_idx) || !json_is_array(page_val))
            continue;

        sscanf(obj_idx, "%d", &page_idx);
        env->page_num = page_idx;

        fz_try(env->ctx) {
            env->page = pdf_load_page(env->ctx, env->doc, page_idx);
        } fz_catch(env->ctx) {
            fprintf(stderr, "cannot get pages: %s\n", fz_caught_message(env->ctx));
            goto data_exit;
        }

        int updated_pg = 0;

        json_array_foreach(page_val, item_idx, env->fill.json_map_item) {
            updated_pg += cmplt_fill_field(env);
        }

        if(updated_pg)
            pdf_update_page(env->ctx, env->page);

        updated_doc += updated_pg;
    }

    if(updated_doc) {
        pdf_finish_edit(env->ctx, env->doc);

        pdf_write_options opts = {0};
        opts.do_incremental = 0;

        pdf_save_document(env->ctx, env->doc, env->files.output, &opts);
    }


    if(env->add_sig) {
        cmplt_sign_and_save(env);
    }

tpl_exit:
    json_decref(template);

data_exit:
    json_decref(data_json);

    if(env->fill.dataFile)
        fclose(data_file);
}



int cmplt_fill_field(pdf_env *env) {
    json_t *datakey = json_object_get(env->fill.json_map_item, "key");

    if(datakey == NULL)
        return 0;

    if(!json_is_string(datakey))
        return 0;

    env->fill.input_key = json_string_value(datakey);
    json_t *dataval = json_object_get(env->fill.json_input_data, env->fill.input_key);

    if(dataval == NULL)
        return 0;

    switch(json_typeof(dataval)) {
        case JSON_STRING:
            env->fill.input_data = json_string_value(dataval);
            break;

        case JSON_TRUE:
        case JSON_FALSE:
            env->fill.input_data = json_is_true(dataval) ? "1" : "0";
            break;

        default:
            return 0;

    }

    if(env->fill.input_data == NULL)
        return 0;

    int updated = 0;
    pdf_widget *widget;

    switch(map_input_data(env)) {
        case FIELD_ID:
            widget = cmplt_find_widget_id(env->ctx, env->page, env->fill.field_id);
            updated = cmplt_set_widget_value(env, widget, env->fill.input_data);
            break;

        case FIELD_NAME:
            widget = cmplt_find_widget_name(env->ctx, env->page, env->fill.field_name);
            updated = cmplt_set_widget_value(env, widget, env->fill.input_data);
            break;

        case ADD_TEXTFIELD:
            updated = cmplt_add_textfield(env);
            break;

        case ADD_SIGNATURE:
            // delay signing until all fields complete in cmplt_fill_all, then save, reload, sign, resave
            env->add_sig = 1;
            memcpy(&env->add_sig_data, &env->fill.sig, sizeof(signature_data));
            break;

        case ADD_IMAGE:
            updated = cmplt_add_image(env);
            break;

        default:
            break;
    }

    return updated;
}

int cmplt_fcopy(const char *src, const char *dest) {
    FILE *in = fopen(src, "r");
    FILE *out = fopen(dest, "w");
    char buf[CP_BUFSIZE];
    int numbytes;

    while(0 < (numbytes = fread(buf, 1, CP_BUFSIZE, in)))
        fwrite(buf, 1, numbytes, out);

    fclose(in);
    fclose(out);
}


void cmplt_set_field_readonly(fz_context *ctx, pdf_document *doc, pdf_obj *field) {
    int ffval = pdf_to_int(ctx, pdf_dict_get(ctx, field, PDF_NAME_Ff));
    pdf_obj *ffobj = pdf_new_int(ctx, doc, ffval | Ff_ReadOnly);
    pdf_dict_put_drop(ctx, field, PDF_NAME_Ff, ffobj);
}



int cmplt_add_image(pdf_env *env) {
    // adapted from https://github.com/rk700/PyMuPDF/blob/master/fitz/fitz_wrap.c fz_page_s_insertImage
    char X[15], Y[15], W[15], H[15], size_str[15], xref_str[15], name[50];
    const char *template = " q %s 0 0 %s %s %s cm /%s Do Q \n";
    const char *name_templ = "Image%s-%s-%s-%s";
    fz_buffer *res = NULL;
    fz_buffer *nres = NULL;
    pdf_obj *resources, *subres, *contents, *ref;
    fz_image *img;
    size_t c_len;
    unsigned char *content_str;
    image_data *imgdata = &env->fill.img;
    struct fz_rect_s prect, rect = {imgdata->pos.left, imgdata->pos.top, imgdata->pos.right, imgdata->pos.bottom};
//    fz_bound_page(env->doc, env->page, &prect);

    fz_try(env->ctx) {
        contents = pdf_dict_get(env->ctx, env->page->obj, PDF_NAME_Contents);
        resources = pdf_dict_get(env->ctx, env->page->obj, PDF_NAME_Resources);
        subres = pdf_dict_get(env->ctx, resources, PDF_NAME_XObject);
        if (!subres) {  // has no XObject yet
            subres = pdf_new_dict(env->ctx, env->doc, 10);
            pdf_dict_put_drop(env->ctx, resources, PDF_NAME_XObject, subres);
        }
        img = fz_new_image_from_file(env->ctx, imgdata->file_name);
        ref = u_pdf_add_image(env->ctx, env->doc, img, 0);

        if(rect.x1 == 0) {
            rect.x1 = img->w;
        }

        if(rect.y1 == 0) {
            rect.y1 = img->h;
        }

        snprintf(X, 15, "%g", (double) rect.x0);
        snprintf(Y, 15, "%g", (double) rect.y0);
        snprintf(W, 15, "%g", (double) rect.x1);
        snprintf(H, 15, "%g", (double) rect.y1);

        snprintf(size_str, 15, "%i", (int) fz_image_size(env->ctx, img));
        snprintf(xref_str, 15, "%i", (int) pdf_to_num(env->ctx, env->page->obj));
        snprintf(name, 50, name_templ, size_str, xref_str, X, Y);
        pdf_dict_puts(env->ctx, subres, name, ref);
        // retrieve and update contents stream
        if (pdf_is_array(env->ctx, contents)) {            // take last if more than one contents object
            int i = pdf_array_len(env->ctx, contents) - 1;
            contents = pdf_array_get(env->ctx, contents, i);
        }

        res = pdf_load_stream(env->ctx, contents);
        if (!res)
            fz_throw(env->ctx, FZ_ERROR_GENERIC, "bad PDF: Contents is no stream object");

        fz_buffer_printf(env->ctx, res, template, W, H, X, Y, name);
        fz_write_buffer_byte(env->ctx, res, 0);
        pdf_dict_put(env->ctx, contents, PDF_NAME_Filter, PDF_NAME_FlateDecode);
        c_len = fz_buffer_storage(env->ctx, res, &content_str);
        nres = cmplt_deflatebuf(env->ctx, content_str, (size_t) c_len);
        pdf_update_stream(env->ctx, env->doc, contents, nres, 1);

    } fz_always(env->ctx) {
        if (img) fz_drop_image(env->ctx, img);
        if (res) fz_drop_buffer(env->ctx, res);
        if (nres) fz_drop_buffer(env->ctx, nres);
    } fz_catch(env->ctx) {
        return 0;
    }

    return 1;
}

fz_buffer *cmplt_deflatebuf(fz_context *ctx, unsigned char *p, size_t n) {
    fz_buffer *buf;
    unsigned long csize;
    int t;
    unsigned long longN = (unsigned long)n;
    unsigned char *data;
    size_t cap;

    if (n != (size_t)longN)
        fz_throw(ctx, FZ_ERROR_GENERIC, "Buffer to large to deflate");

    cap = compressBound(longN);
    data = fz_malloc(ctx, cap);
    buf = fz_new_buffer_from_data(ctx, data, cap);
    csize = (uLongf)cap;
    t = compress(data, &csize, p, longN);
    if (t != Z_OK)
    {
        fz_drop_buffer(ctx, buf);
        fz_throw(ctx, FZ_ERROR_GENERIC, "cannot deflate buffer");
    }
    fz_resize_buffer(ctx, buf, csize);
    return buf;
}


int cmplt_da_str(const char *font, float *color, char *buf) {
    int size = 0;
    char tmp_fn[30];
    sscanf(font, "%s %d", tmp_fn, &size);

    if(size <= 0) {
        size = DEFAULT_FONT_HEIGHT;
    }

    if(color == NULL) {
        return sprintf(buf, "/%s %d Tf 0 g", tmp_fn, size);
    } else {
        return sprintf(buf, "/%s %d Tf %.2f %.2f %.2f gb", tmp_fn, size, color[0], color[1], color[2]);
    }
}


int cmplt_add_signature(fz_context *ctx, pdf_document *doc, pdf_page *page, signature_data *sig) {
    pdf_widget *widget = pdf_create_widget(ctx, doc, page, PDF_WIDGET_TYPE_SIGNATURE, (char *) sig->widget_name);

    pdf_annot *annot = (pdf_annot*) widget;

    char fn_str[50];
    if(sig->visible != 0 && cmplt_da_str(sig->font, NULL, fn_str) > 0) {
        fz_rect *rect = (fz_rect *) &sig->pos;

        pdf_set_annot_rect(ctx, annot, rect);
        pdf_obj *da_pdf = pdf_new_string(ctx, doc, fn_str, strlen(fn_str));
        pdf_dict_put_drop(ctx, annot->obj, PDF_NAME_DA, da_pdf);
        pdf_field_set_display(ctx, doc, annot->obj, 0);
    }

    pdf_sign_signature(ctx, doc, widget, sig->file, sig->password);

    return 1;
}


int cmplt_add_textfield(pdf_env *env) {
    pdf_widget *widget = pdf_create_widget(env->ctx, env->doc, env->page, PDF_WIDGET_TYPE_TEXT, (char*)env->fill.input_key);

    fz_rect *rect = (fz_rect *) &env->fill.text.pos;

    pdf_annot *annot = (pdf_annot*) widget;

    pdf_set_annot_rect(env->ctx, annot, rect);
    pdf_field_set_value(env->ctx, env->doc, annot->obj, env->fill.input_data);
    pdf_field_set_display(env->ctx, env->doc, annot->obj, 0);

    char fn_str[50];
    if(env->fill.text.font && cmplt_da_str(env->fill.text.font, env->fill.text.color, fn_str) > 0) {
        pdf_obj *da_pdf= pdf_new_string(env->ctx, env->doc, fn_str, strlen(fn_str));
        pdf_dict_put_drop(env->ctx, annot->obj, PDF_NAME_DA, da_pdf);
    }

    if(!env->fill.text.editable) {
        cmplt_set_field_readonly(env->ctx, env->doc, annot->obj);
    }

    return 1;
}


pdf_widget *cmplt_find_widget_id(fz_context *ctx, pdf_page *page, int field_id) {
    pdf_annot *annot = page->annots;

    while(annot) {
        if(pdf_annot_type(ctx, annot) != PDF_ANNOT_WIDGET)
            continue;

        if(pdf_to_num(ctx, annot->obj) == field_id)
            return (pdf_widget *) annot;

        annot = annot->next;
    }

    return NULL;
}


pdf_widget *cmplt_find_widget_name(fz_context *ctx, pdf_page *page, const char *field_name) {
    pdf_annot *annot = page->annots;

    while(annot) {
        if(pdf_annot_type(ctx, annot) != PDF_ANNOT_WIDGET)
            continue;

        char *utf8_name = UTF8_FIELD_NAME(ctx, annot->obj);
        int cmp = strcmp(utf8_name, field_name);

        if(cmp == 0)
            return (pdf_widget *) annot;

        annot = annot->next;
    }

    return NULL;
}


int cmplt_set_widget_value(pdf_env *env, pdf_widget *widget, const char *data) {
    return widget == NULL ? 0 : pdf_field_set_value(env->ctx, env->doc, ((pdf_annot*) widget)->obj, data);
}


int str_is_all_digits(const char *str) {
    while(*str)
        if(!isdigit(*str++))
            return 0;

    return 1;
}


static int cmplt_sign_and_save(pdf_env *env) {
    int retval = 1;
    pdf_document *sig_doc;
    pdf_page *sig_page;
    fz_context *sig_ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);

    if (!sig_ctx) {
        fprintf(stderr, "cannot create mupdf context\n");
        retval = 0;
        goto sig_exit;
    }

    /* Only handle pdf. */
    fz_try(sig_ctx) {
        fz_register_document_handler(sig_ctx, &pdf_document_handler);
    } fz_catch(sig_ctx) {
        fprintf(stderr, "cannot register document handlers: %s\n", fz_caught_message(sig_ctx));
        retval = 0;
    }

    if(!retval) goto sig_exit_ctxt;

    /* Open the document. */
    fz_var(retval);
    fz_try(sig_ctx) {
        sig_doc = pdf_open_document(sig_ctx, env->files.output);
    } fz_catch(sig_ctx)	{
        fprintf(stderr, "cannot open document: %s\n", fz_caught_message(sig_ctx));
        retval = 0;
    }

    if(!retval) goto sig_exit_ctxt;

    fz_try(sig_ctx) {
        sig_page = pdf_load_page(sig_ctx, sig_doc, env->fill.sig.page_num);
    } fz_catch(sig_ctx) {
        fprintf(stderr, "cannot get page: %s\n", fz_caught_message(sig_ctx));
        retval = 0;
    }

    if(!retval) goto sig_exit_doc;

    cmplt_add_signature(sig_ctx, sig_doc, sig_page, &env->add_sig_data);
    pdf_update_page(sig_ctx, sig_page);
    pdf_write_options sig_opts = {0};
    sig_opts.do_incremental = 1;
    pdf_save_document(sig_ctx, sig_doc, env->files.output, &sig_opts);

sig_exit_doc:
    pdf_drop_document(sig_ctx, sig_doc);
sig_exit_ctxt:
    fz_drop_context(sig_ctx);

sig_exit:
    return retval;
}
