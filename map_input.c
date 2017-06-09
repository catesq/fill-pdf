#include "fill.h"

static double map_input_number(json_t *jsn_obj, const char *property, float default_val, float min_val) {
    json_t *jsn_val = json_object_get(jsn_obj, property);

    if(json_is_number(jsn_val) && json_number_value(jsn_val) >= min_val) {
        return json_number_value(jsn_val);
    } else {
       return default_val;
    }
}

static int map_data_font(json_t *obj, const char **fontname, float *fontsize, const char **fontpath) {
    json_t *json_font, *json_sz;

    json_font = json_object_get(obj, "font");
    if(json_is_string(json_font)) {
        *fontname = json_string_value(json_font);
    } else {
        *fontname = NULL;
    }

    json_sz = json_object_get(obj, "fontsize");
    if(json_is_number(json_sz)) {
        *fontsize = (float) json_number_value(json_sz);
    } else {
        *fontsize = (float) DEFAULT_TEXT_HEIGHT;
    }

    if(!fontpath) {
        return 1;
    }

    json_font = json_object_get(obj, "fontpath");
    if(json_is_string(json_font)) {
        *fontpath = json_string_value(json_font);
    } else {
        *fontpath = *fontname;
    }

    return 1;
}


static int map_input_rectpos(json_t *prt_item, pos_data *pos, const char *rect_pty, const char *pos_pty, float default_width, float default_height) {
    json_t *json_pos;

    if(rect_pty && (json_pos = json_object_get(prt_item, rect_pty)) != NULL) {
        pos->left = map_input_number(json_pos, "left", 0, 0);
        pos->top = map_input_number(json_pos, "top", 0, 0);
        pos->width = map_input_number(json_pos, "width", default_width, 0);
        pos->height = map_input_number(json_pos, "height", default_height, 0);
    } else if(pos_pty && (json_pos = json_object_get(prt_item, pos_pty)) != NULL) {
        pos->left = map_input_number(json_pos, "left", 0, 0);
        pos->top = map_input_number(json_pos, "top", 0, 0);
        pos->width = 0;
        pos->height = 0;
    } else {
        return 0;
    }

    return 1;
}

static int map_color_value(json_t *jsn_obj, const char *property, float *color, float rgb_default, float alpha_default) {
    json_t *json_color, *json_a;

    json_color = json_object_get(jsn_obj, property);

    color[3] = alpha_default;
    color[0] = color[1] = color[2] = rgb_default;

    if(!json_is_array(json_color) || json_array_size(json_color) < 3) {
        return 0;
    }

    color[0] = json_number_value(json_array_get(json_color, 0));
    color[1] = json_number_value(json_array_get(json_color, 1));
    color[2] = json_number_value(json_array_get(json_color, 2));

    if((json_a = json_array_get(json_color, 3)) != NULL) {
        color[3] = json_number_value(json_a);
    }

    return 1;
}

fill_type map_input_signature(pdf_env *env) {
    const char *sigfile = NULL;
    json_t *json_sigfile = NULL, *json_font = NULL, *json_sz = NULL, *json_text = NULL, *json_pwd;
    struct stat buffer;

    if(!map_input_rectpos(env->fill.json_map_item, &env->fill.sig.pos, "rect", "pos", DEFAULT_SIG_WIDTH, DEFAULT_SIG_HEIGHT)) {
        RETURN_FILL_ERROR("Pos/rect item for signature not given");
    }

    env->fill.sig.widget_name = env->fill.input_key;
    env->fill.sig.page_num = env->page_num;
    env->fill.sig.visible = 1;

    if(!map_data_font(env->fill.json_map_item, &env->fill.sig.font, &env->fill.sig.fontsize, NULL)) {
        RETURN_FILL_ERROR("No font");
        env->fill.sig.font = 0;
    }

    json_sigfile = json_object_get(env->fill.json_map_item, "sigfile");
    if(env->fill.certFile) {
        sigfile = env->fill.certFile;
    } else if(json_is_string(json_sigfile)) {
        sigfile = json_string_value(json_sigfile);
    } else {
        RETURN_FILL_ERROR("Sigfile not set");
    }

    if(stat(sigfile, &buffer) != 0) {
        RETURN_FILL_ERROR_ARG("Sigfile %s not found", sigfile);
    } else {
        env->fill.sig.file = sigfile;
    }

    json_pwd = json_object_get(env->fill.json_map_item, "password");
    if(env->fill.certPwd) {
        env->fill.sig.password = env->fill.certPwd;
    } else if(json_is_string(json_pwd)) {
        env->fill.sig.password = json_string_value(json_pwd);
    } else {
        RETURN_FILL_ERROR("Password not given");
    }

    json_text = json_object_get(env->fill.json_map_item, "text");
    env->fill.sig.text = json_is_string(json_text) ? json_string_value(json_text) : NULL;

    json_text = json_object_get(env->fill.json_map_item, "gfx");
    env->fill.sig.gfx = json_is_string(json_text) ? json_string_value(json_text) : NULL;

    return ADD_SIGNATURE;
}


fill_type map_input_textfield(pdf_env *env) {
    pos_data *pos = &env->fill.text.pos;
    json_t *json_pos = json_object_get(env->fill.json_map_item, "rect");


    if(!map_input_rectpos(env->fill.json_map_item, &env->fill.text.pos, "rect", NULL, DEFAULT_TEXT_WIDTH, DEFAULT_TEXT_HEIGHT)) {
        RETURN_FILL_ERROR("Pos/rect item for textfield not given");
    } else if(env->fill.text.pos.left < 0 || env->fill.text.pos.top < 0) {
        RETURN_FILL_ERROR("Position invalid");
    }

    json_t *json_edit = json_object_get(env->fill.json_map_item, "editable");
    if(json_is_boolean(json_edit)) {
        env->fill.text.editable = json_boolean_value(json_edit);
    } else {
        env->fill.text.editable = 0;
    }


    if(!map_data_font(env->fill.json_map_item, &env->fill.text.font, &env->fill.text.fontsize, NULL)) {
        env->fill.text.font = 0;
    }

    map_color_value(env->fill.json_map_item, "color", env->fill.text.color, 0, 1);

    return ADD_TEXTFIELD;
}



fill_type map_input_text(pdf_env *env) {
    if(!map_data_font(env->fill.json_map_item, &env->fill.text.font, &env->fill.text.fontsize, &env->fill.text.fontfile)) {
        RETURN_FILL_ERROR("Missing fontname for text item");
    }

    if(!map_input_rectpos(env->fill.json_map_item, &env->fill.text.pos, NULL, "pos", 0, 0)) {
        RETURN_FILL_ERROR("Need to define pos for text item");
    }

    map_color_value(env->fill.json_map_item, "color", env->fill.text.color, 0, 1);

    return ADD_TEXT;
}

fill_type map_input_image(pdf_env *env) {
    pos_data *pos = &env->fill.img.pos;
    json_t *json_pos;


    if(!map_input_rectpos(env->fill.json_map_item, &env->fill.img.pos, "rect", "pos", 0, 0)) {
        RETURN_FILL_ERROR("Image position not set");
    }

    json_t *json_fname = json_object_get(env->fill.json_map_item, "src");
    env->fill.img.file_name = json_string_value(json_fname);

    return ADD_IMAGE;
}

fill_type map_input_data(pdf_env *env) {
    json_t *json_id = json_object_get(env->fill.json_map_item, "id");
    if(json_id != NULL) {
        if (!json_is_integer(json_id)) {
            RETURN_FILL_ERROR("Invalid field id, must be an integer");
        }

        env->fill.field_id = json_integer_value(json_id);
        return FIELD_ID;
    }

    json_t *json_name = json_object_get(env->fill.json_map_item, "name");
    if(json_name != NULL) {
        if (!json_is_string(json_name)) {
            RETURN_FILL_ERROR("Invalid field name, must be a string");
        }

        env->fill.field_name = json_string_value(json_name);
        return FIELD_NAME;
    }

    json_t *json_addtype = json_object_get(env->fill.json_map_item, "add");

    if(json_addtype == NULL) {
        RETURN_FILL_ERROR("Unrecognised template object. Needs an 'id', 'name' or 'add' property.");
    }

    const char *type_name = json_string_value(json_addtype);

    if(strncmp(type_name, "textfield", 9) == 0) {
        return map_input_textfield(env);
    } else if (strncmp(type_name, "text", 4) == 0) {
        return map_input_text(env);
    } else if(strncmp(type_name, "signature", 9) == 0) {
        return map_input_signature(env);
    } else if(strncmp(type_name, "text", 5) == 0) {
        RETURN_FILL_ERROR("Text overlay not yet supported");
    } else if(strncmp(type_name, "image", 6) == 0) {
        return map_input_image(env);
    } else if(strncmp(type_name, "signature_logo", 6) == 0) {
        RETURN_FILL_ERROR("Logo signature not yet supported");
    } else {
        RETURN_FILL_ERROR("Trying to add an unrecognised type. Only 'textfield', 'signature', 'text' & 'image' supported.");
    }

}
