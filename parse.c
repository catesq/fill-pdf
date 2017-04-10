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


void visit_doc_init_json(pdf_env *env, visit_env *visenv) {
    visenv->json_root = json_array();
}


void visit_doc_end_json(pdf_env *env, visit_env *visenv) {
    if(env->optFile) {
        json_dump_file(visenv->json_root, env->optFile, JSON_INDENT(2));
    } else {
        json_dumpf(visenv->json_root, stdout, JSON_INDENT(2));
    }

    json_decref(visenv->json_root);
}


void visit_page_init_json(pdf_env *env, visit_env *visenv) {
    visenv->json_item = json_array();
}


void visit_page_end_json(pdf_env *env, visit_env *visenv) {
    json_array_append_new(visenv->json_root, visenv->json_item);
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


void visit_field_jsonmap(pdf_env *env, visit_env *visenv, pdf_widget *widget, int widget_num) {
    json_t *obj = visit_field_json_shared(env->ctx, env->doc, widget);

    json_object_set_new(obj, "key", json_string(""));
    json_array_append_new(visenv->json_item, obj);
}


void visit_field_jsonlist(pdf_env *env, visit_env *visenv, pdf_widget *widget, int widget_num) {
    json_t *jsobj = visit_field_json_shared(env->ctx, env->doc, widget);

    json_object_set_new(jsobj, "type", json_string(get_type_name(env->ctx, widget)));

    fz_rect rect;
    pdf_annot_rect(env->ctx, (pdf_annot *) widget, &rect);

    json_t *bounds = json_object();
    json_object_set_new(bounds, "left", json_real(rect.x0));
    json_object_set_new(bounds, "top", json_real(rect.y0));
    json_object_set_new(bounds, "right", json_real(rect.x1));
    json_object_set_new(bounds, "bottom", json_real(rect.y1));

    json_object_set_new(jsobj, "bounds", bounds);

    int maxlen = pdf_text_widget_max_len(env->ctx, env->doc, widget);
    if(maxlen > 0) {
        json_object_set_new(jsobj, "maxlen", json_integer(maxlen));
    }

    json_array_append_new(visenv->json_item, jsobj);
}




void visit_field_overlay(pdf_env *env, visit_env *visenv, pdf_widget *widget, int widget_num) {
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


void visit_page_end_overlay(pdf_env *env, visit_env *visenv) {
    pdf_update_page(env->ctx, env->page);
}


void visit_doc_end_overlay(pdf_env *env, visit_env *visenv) {
    pdf_write_options opts = {0};
    opts.do_incremental = 0;
    pdf_save_document(env->ctx, env->doc, env->optFile, &opts);
}


visit_funcs get_visitor_funcs(int cmd) {
    switch(cmd) {
        case JSON_MAP: {
            visit_funcs vf = {visit_doc_init_json, visit_page_init_json, visit_field_jsonmap, visit_page_end_json, visit_doc_end_json};
            return vf;
        }

        case JSON_LIST: {
            visit_funcs vf = {visit_doc_init_json, visit_page_init_json, visit_field_jsonlist, visit_page_end_json, visit_doc_end_json};
            return vf;
        }

        case FIELD_OVERLAY: {
            visit_funcs vf = {0, 0, visit_field_overlay, visit_page_end_overlay, visit_doc_end_overlay};
            return vf;
        }
    }
}


void parse_fields_doc(pdf_env *env) {
    visit_funcs vfuncs = get_visitor_funcs(env->cmd);
    visit_env visenv = {0};

    fz_try(env->ctx) {
        if(vfuncs.pre_visit_doc)
            vfuncs.pre_visit_doc(env, &visenv);

        for(int i = 0; i < env->page_count; i++) {
            env->page_num = i;
            env->page = pdf_load_page(env->ctx, env->doc, env->page_num);

            if(vfuncs.pre_visit_page)
                vfuncs.pre_visit_page(env, &visenv);

            pdf_widget *widget = pdf_first_widget(env->ctx, env->doc, env->page);
            int wid_count = 0;

            while(widget) {
                if(vfuncs.visit_field)
                    vfuncs.visit_field(env, &visenv, widget, wid_count);

                widget = pdf_next_widget(env->ctx, widget);
                wid_count++;
            }

            if(vfuncs.post_visit_page)
                vfuncs.post_visit_page(env, &visenv);
        }

        if(vfuncs.post_visit_doc)
            vfuncs.post_visit_doc(env, &visenv);

    } fz_catch(env->ctx) {
        fprintf(stderr, "cannot get pages: %s\n", fz_caught_message(env->ctx));
    }

}
