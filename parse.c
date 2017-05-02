#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
#include <jansson.h>
#include "fill.h"


static const char *typeNames[] = { "pushbutton", "checkbox", "radiobutton", "textfield", "listbox", "combobox", "signature" };


static const char *get_type_name(fz_context *ctx, pdf_widget *widget) {
    int type = pdf_widget_type(ctx, widget);

    if(type >=0 && type <=6) {
        return typeNames[type];
    } else {
        return "";
    }
}


void parse_fields_doc(pdf_env *env) {
    visit_funcs vfuncs = get_visitor_funcs(env->cmd);

    fz_try(env->ctx) {
        if(vfuncs.pre_visit_doc)
            vfuncs.pre_visit_doc(env);

        for(int i = 0; i < env->page_count; i++) {
            env->page_num = i;
            env->page = pdf_load_page(env->ctx, env->doc, env->page_num);

            if(vfuncs.pre_visit_page)
                vfuncs.pre_visit_page(env);

            pdf_widget *widget = pdf_first_widget(env->ctx, env->doc, env->page);
            int wid_count = 0;

            while(widget) {
                if(vfuncs.visit_widget)
                    vfuncs.visit_widget(env, widget, wid_count);

                widget = pdf_next_widget(env->ctx, widget);
                wid_count++;
            }

            if(vfuncs.post_visit_page)
                vfuncs.post_visit_page(env);
        }

        if(vfuncs.post_visit_doc)
            vfuncs.post_visit_doc(env);

    } fz_catch(env->ctx) {
        fprintf(stderr, "cannot get pages: %s\n", fz_caught_message(env->ctx));
    }
}


visit_funcs get_visitor_funcs(int cmd) {
    switch(cmd) {
        case JSON_MAP: {
            visit_funcs vf = {visit_doc_init_json, visit_page_init_json, visit_widget_jsonmap, visit_page_end_json, visit_doc_end_json};
            return vf;
        }

        case JSON_LIST: {
            visit_funcs vf = {visit_doc_init_json, visit_page_init_json, visit_field_jsonlist, visit_page_end_json, visit_doc_end_json};
            return vf;
        }

        case FONT_LIST: {
            visit_funcs vf = {visit_doc_init_json, visit_page_fontlist, visit_widget_fontlist, 0, visit_doc_end_json};
            return vf;
        }

        case ANNOTATE_FIELDS: {
            visit_funcs vf = {0, 0, visit_widget_overlay, visit_page_end_overlay, visit_doc_end_overlay};
            return vf;
        }
    }
}


void visit_doc_init_json(pdf_env *env) {
    env->parse.json_root = json_object();
}


void visit_doc_end_json(pdf_env *env) {
    if(env->files.output) {
        fprintf(stderr, "Saving json for '%s' command to '%s'\n", command_name(env->cmd), env->files.output);
        json_dump_file(env->parse.json_root, env->files.output, JSON_INDENT(2));
    } else {
        json_dumpf(env->parse.json_root, stdout, JSON_INDENT(2));
    }

    json_decref(env->parse.json_root);
}


json_t *build_font_json(fz_context *ctx, pdf_obj *font_dict) {
    json_t *font = json_object();

    int len = pdf_dict_len(ctx, font_dict);

    for(int i = 0; i < len; i++) {
        pdf_obj *key = pdf_dict_get_key(ctx, font_dict, i);
        pdf_obj *val = pdf_dict_get_val(ctx, font_dict, i);

        if(pdf_is_name(ctx, val))
            json_object_set_new(font, pdf_to_name(ctx, key), json_string(pdf_to_name(ctx, val)));
    }

    json_object_set_new(font, "pages", json_array());

    return font;
}

void visit_page_fontlist(pdf_env *env) {
    pdf_obj *dict = pdf_page_resources(env->ctx, env->page);

    if(!pdf_is_dict(env->ctx, dict))
        return;

    int dlen = pdf_dict_len(env->ctx, dict);

    if(dlen <= 0)
        return;

    if(!json_object_get(env->parse.json_root, "page_fonts")) {
        json_object_set_new(env->parse.json_root, "page_fonts", json_object());
        json_object_set_new(env->parse.json_root, "widget_fonts", json_object());
    }

    json_t *pg_res_fonts = json_object_get(env->parse.json_root, "page_fonts");

    for(int i = 0; i < dlen; i++) {
        pdf_obj *key = pdf_dict_get_key(env->ctx, dict, i);
        pdf_obj *val = pdf_dict_get_val(env->ctx, dict, i);

        if(!pdf_is_name(env->ctx, key) || !pdf_is_dict(env->ctx, val))
            continue;

        char *keyname = pdf_to_name(env->ctx, key);

        if(strncmp(keyname, "Font", 4) != 0)
            continue;

        int silen = pdf_dict_len(env->ctx, val);
        for(int j = 0; j < silen; j++) {
            pdf_obj *subkey = pdf_dict_get_key(env->ctx, val, j);
            char *font_id = pdf_to_name(env->ctx, subkey);

            pdf_obj *subval = pdf_dict_get_val(env->ctx, val, j);
            if(!pdf_is_dict(env->ctx, subval)) {
                continue;
            }

            if(!json_object_get(pg_res_fonts, font_id)) {
                json_object_set_new(pg_res_fonts, font_id, build_font_json(env->ctx, subval));
            }

            json_t *fonts = json_object_get(pg_res_fonts, font_id);
            json_t *pages = json_object_get(fonts, "pages");
            json_array_append_new(pages, json_integer(env->page_num));
        }
    }
}

void visit_widget_fontlist(pdf_env *env, pdf_widget *widget, int widget_num) {
    pdf_obj *obj = ((pdf_annot *) widget)->obj;

    pdf_obj *ap = pdf_dict_get(env->ctx, obj, PDF_NAME_AP); //(AP)pearance dictionary
    if(!ap || !pdf_is_dict(env->ctx, ap)) return;

    pdf_obj *ref = pdf_dict_get(env->ctx, ap, PDF_NAME_N); //(N)ormal appearance ref
    if(!ref || !pdf_is_indirect(env->ctx, ref)) return;

    pdf_obj *norm_ap = pdf_resolve_indirect(env->ctx, ref); //Norm ap obj
    if(!norm_ap || !pdf_is_dict(env->ctx, norm_ap)) return;

    pdf_obj *norm_res = pdf_dict_get(env->ctx, norm_ap, PDF_NAME_Resources); //Resources of normal appearance
    if(!norm_res || !pdf_is_dict(env->ctx, norm_res)) return;

    pdf_obj *fonts = pdf_dict_get(env->ctx, norm_res, PDF_NAME_Font);  //here we go
    if(!fonts || !pdf_is_dict(env->ctx, fonts)) return;

    int fonts_len = pdf_dict_len(env->ctx, fonts);
    if(fonts_len == 0) return;

    json_t *widget_fonts = json_object_get(env->parse.json_root, "widget_fonts");

    for(int i = 0; i < fonts_len; i++) {
        pdf_obj *key = pdf_dict_get_key(env->ctx, fonts, i);
        char *fn_name = pdf_to_name(env->ctx, key);

        pdf_obj *font_ref = pdf_resolve_indirect(env->ctx, pdf_dict_get_val(env->ctx, fonts, i));
        pdf_obj *base_font = pdf_dict_get(env->ctx, font_ref, PDF_NAME_BaseFont);

        json_object_set_new(widget_fonts, fn_name, json_string(pdf_to_name(env->ctx, base_font)));
    }
}


void visit_page_init_json(pdf_env *env) {
    env->parse.json_item = json_array();
}


void visit_page_end_json(pdf_env *env) {
    if(!json_is_array(env->parse.json_item) || json_array_size(env->parse.json_item) == 0)
        return;

    char buf[10];
    snprintf(buf, 10, "%d", env->page_num);
    json_object_set(env->parse.json_root, buf, env->parse.json_item);
}


json_t *visit_field_json_shared(fz_context *ctx, pdf_document *doc, pdf_widget *widget) {
    json_t *jsobj = json_object();
    pdf_annot *annot = (pdf_annot*) widget;

    int obj_num = pdf_to_num(ctx, annot->obj);
    json_object_set_new(jsobj, "id", json_integer(obj_num));

    char *utf8_name = UTF8_FIELD_NAME(ctx, annot->obj);
    json_object_set_new(jsobj, "name", json_string(utf8_name));
    fz_free(ctx, utf8_name);

    return jsobj;
}


void visit_widget_jsonmap(pdf_env *env, pdf_widget *widget, int widget_num) {
    json_t *obj = visit_field_json_shared(env->ctx, env->doc, widget);

    json_object_set_new(obj, "key", json_string(""));
    json_array_append_new(env->parse.json_item, obj);
}


void visit_field_jsonlist(pdf_env *env, pdf_widget *widget, int widget_num) {
    json_t *jsobj = visit_field_json_shared(env->ctx, env->doc, widget);

    json_object_set_new(jsobj, "type", json_string(get_type_name(env->ctx, widget)));

    fz_rect rect;
    pdf_annot_rect(env->ctx, (pdf_annot *) widget, &rect);


    int maxlen = pdf_text_widget_max_len(env->ctx, env->doc, widget);
    if(maxlen > 0) {
        json_object_set_new(jsobj, "maxlen", json_integer(maxlen));
    }

    json_t *bounds = json_object();
    json_object_set_new(bounds, "left", json_real(rect.x0));
    json_object_set_new(bounds, "top", json_real(rect.y0));
    json_object_set_new(bounds, "right", json_real(rect.x1));
    json_object_set_new(bounds, "bottom", json_real(rect.y1));

    json_object_set_new(jsobj, "rect", bounds);

    json_array_append_new(env->parse.json_item, jsobj);
}



void visit_widget_overlay(pdf_env *env, pdf_widget *widget, int widget_num) {
    fz_rect rect;
    pdf_annot *overlay = pdf_create_annot(env->ctx, env->page, pdf_annot_type_from_string("FreeText"));
    pdf_bound_widget(env->ctx, widget, &rect);

    int objnum = pdf_to_num(env->ctx, ((pdf_annot *) widget)->obj);
    char buf[8];
    snprintf(buf, 8, "%d", objnum);
    pdf_set_annot_contents(env->ctx, overlay, buf);

    fz_point pt = {rect.x0, (rect.y0 + rect.y1) / 2.0f};
    float col[] = {0.8,0,.0f};
    pdf_set_free_text_details(env->ctx, overlay, &pt, buf, "Courier-Bold", 10, col);
}


void visit_page_end_overlay(pdf_env *env) {
    pdf_update_page(env->ctx, env->page);
}


void visit_doc_end_overlay(pdf_env *env) {
    pdf_write_options opts = {0};
    opts.do_incremental = 0;
    char *output_file;
    char buf[256];
    int len;

    if(env->files.output) {
        output_file = env->files.output;
    } else {
        len = strlen(env->files.input);
        len = (len < 240) ? len - 4 : 240;

        memcpy(buf, env->files.input, len);
        memcpy(buf+len, "_annotated.pdf", 15);
        output_file = buf;
    }

    fprintf(stderr, "Writing annoted pdf to %s\n", output_file);
    pdf_save_document(env->ctx, env->doc, output_file, &opts);
}
