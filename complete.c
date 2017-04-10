#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "fill.h"


void cmplt_set_field_readonly(fz_context *ctx, pdf_document *doc, pdf_obj *field) {
    int ffval = pdf_to_int(ctx, pdf_dict_get(ctx, field, PDF_NAME_Ff));
    pdf_obj *ffobj = pdf_new_int(ctx, doc, ffval | Ff_ReadOnly);
    pdf_dict_put_drop(ctx, field, PDF_NAME_Ff, ffobj);
}


int cmplt_add_signature(pdf_env *env, fill_env *fillenv) {
    pdf_widget *widget = pdf_create_widget(env->ctx, env->doc, env->page, PDF_WIDGET_TYPE_SIGNATURE, (char *) fillenv->input_key);

    pdf_annot *annot = (pdf_annot*) widget;
    fz_rect rect = {0,0,50,50};
    pdf_set_annot_rect(env->ctx, annot, &rect);

    char *fn_str = "/Helv 9 Tf 0 g";
    pdf_obj *da_pdf = pdf_new_string(env->ctx, env->doc, fn_str, strlen(fn_str));
    pdf_dict_put_drop(env->ctx, annot->obj, PDF_NAME_DA, da_pdf);
    pdf_field_set_display(env->ctx, env->doc, annot->obj, 0);

    pdf_sign_signature(env->ctx, env->doc, widget, fillenv->sig.file, fillenv->sig.password);

    return 1;
}


int cmplt_da_str(text_data *data, char *buf) {
    int size = 0;
    char fn[30];
    sscanf(data->font, "%s %d", fn, &size);

    if(size <= 0) {
        size = DEFAULT_FONT_HEIGHT;
    }

    return sprintf(buf, "/%s %d Tf %.2f %.2f %.2f gb", fn, size, data->color[0], data->color[1], data->color[2]);
}


int cmplt_add_textfield(pdf_env *env, fill_env *fillenv) {
    int len = strlen(fillenv->input_key);
    char *buf = malloc(len + 5);
    memcpy(buf, fillenv->input_key, len);
    memcpy(buf+len, "[0]\0", 5);

    pdf_widget *widget = pdf_create_widget(env->ctx, env->doc, env->page, PDF_WIDGET_TYPE_TEXT, buf);

    fz_rect rect = {
        fillenv->text.x,
        fillenv->text.y,
        fillenv->text.x + fillenv->text.w,
        fillenv->text.y + fillenv->text.h
    };

    pdf_annot *annot = (pdf_annot*) widget;

    pdf_set_annot_rect(env->ctx, annot, &rect);
    pdf_field_set_value(env->ctx, env->doc, annot->obj, fillenv->input_data);
    pdf_field_set_display(env->ctx, env->doc, annot->obj, 0);

    char fn_str[50];
    if(fillenv->text.font && cmplt_da_str(&fillenv->text, fn_str) > 0) {
        pdf_obj *da_pdf= pdf_new_string(env->ctx, env->doc, fn_str, strlen(fn_str));
        pdf_dict_put_drop(env->ctx, annot->obj, PDF_NAME_DA, da_pdf);
    }

    if(!fillenv->text.editable) {
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
        fz_free(ctx, utf8_name);

        if(cmp == 0)
            return (pdf_widget *) annot;

        annot = annot->next;
    }

    return NULL;
}


int cmplt_set_widget_value(pdf_env *env, pdf_widget *widget, const char *data) {
    return widget == NULL ? 0 : pdf_field_set_value(env->ctx, env->doc, ((pdf_annot*) widget)->obj, data);
}


int cmplt_fill_field(pdf_env *env, fill_env *fillenv) {
    json_t *datakey = json_object_get(fillenv->json_map_item, "key");

    if(datakey == NULL)
        return 0;

    if(!json_is_string(datakey))
        return 0;

    fillenv->input_key = json_string_value(datakey);
    json_t *dataval = json_object_get(fillenv->json_input_data, fillenv->input_key);

    if(dataval == NULL)
        return 0;

    switch(json_typeof(dataval)) {
        case JSON_STRING:
            fillenv->input_data = json_string_value(dataval);
            break;

        case JSON_TRUE:
        case JSON_FALSE:
            fillenv->input_data = json_is_true(dataval) ? "1" : "0";
            break;

        default:
            return 0;

    }

    if(fillenv->input_data == NULL)
        return 0;

    int updated = 0;
    pdf_widget *widget;

    switch(fill_tpl_data(env, fillenv)) {
        case FIELD_ID:
            widget = cmplt_find_widget_id(env->ctx, env->page, fillenv->field_id);
            updated = cmplt_set_widget_value(env, widget, fillenv->input_data);
            break;

        case FIELD_NAME:
            widget = cmplt_find_widget_name(env->ctx, env->page, fillenv->field_name);
            updated = cmplt_set_widget_value(env, widget, fillenv->input_data);
            break;

        case ADD_TEXTFIELD:
            updated = cmplt_add_textfield(env, fillenv);
            break;

        case ADD_SIGNATURE:
            updated = cmplt_add_signature(env, fillenv);
            break;

        default:
            fprintf(stderr, "Error in '%s': %s\n", fillenv->input_key, fillenv->err_msg);
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

int is_only_digits(const char *str) {
    while(*str)
        if(!isdigit(*str++))
            return 0;

    return 1;
}

void cmplt_fill_all(pdf_env *env, FILE *json_data_file) {
    json_error_t json_err;
    fill_env fillenv = {0};

    json_t *data_json = json_loadf(json_data_file, 0, &json_err);

    if (data_json == NULL)
        return;

    if (!json_is_object(data_json)) {
        fprintf(stderr, "input data invalid. json root must be an object");
        goto data_exit;
    }

    fillenv.json_input_data = data_json;

    json_t *template = json_load_file(env->optFile, 0, &json_err);

    if (template == NULL) {
        fprintf(stderr, "Unable to load template file '%s'", env->optFile);
        goto tpl_exit;
    }

    if(!json_is_object(template)) {
        fprintf(stderr, "Invalid template file '%s'. json root must be an object.", env->optFile);
        goto tpl_exit;
    }

    const char* obj_idx;
    int page_idx, item_idx;
    json_t *page_val, *item_val;

    int updated_doc = 0;
    json_object_foreach(template, obj_idx, page_val) {
        // filter pages in the tpl.json.
        if(!is_only_digits(obj_idx) || !json_is_array(page_val))
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

        json_array_foreach(page_val, item_idx, fillenv.json_map_item) {
            updated_pg += cmplt_fill_field(env, &fillenv);
        }

        if(updated_pg)
            pdf_update_page(env->ctx, env->page);

        updated_doc += updated_pg;
    }

    cmplt_fcopy(env->inputPdf, env->outputPdf);
    pdf_write_options opts = {0};
    opts.do_incremental = 1;
    pdf_save_document(env->ctx, env->doc, env->outputPdf, &opts);

tpl_exit:
    json_decref(template);

data_exit:
    json_decref(data_json);
}
