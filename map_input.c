#include "fill.h"

double map_input_number(json_t *jsn_obj, const char *property, float default_val, float min_val) {
    json_t *jsn_val = json_object_get(jsn_obj, property);

    if(json_is_number(jsn_val) && json_number_value(jsn_val) >= min_val) {
        return json_number_value(jsn_val);
    } else {
       return default_val;
    }
}

fill_type map_input_signature(pdf_env *env) {
    const char *sigfile = NULL;
    json_t *json_sigfile = NULL, *json_font = NULL, *json_text = NULL, *json_pwd, *json_pos;
    struct stat buffer;
    pos_data *pos = &env->fill.sig.pos;

    json_pos = json_object_get(env->fill.json_map_item, "rect");

    if(!json_is_object(json_pos)) {
        RETURN_FILL_ERROR("Rect for signature not set");
    }

    pos->left = map_input_number(json_pos, "left", 0, 0);
    pos->top = map_input_number(json_pos, "top", 0, 0);
    pos->right = pos->left + map_input_number(json_pos, "width", DEFAULT_SIG_WIDTH, 1);
    pos->bottom = pos->top + map_input_number(json_pos, "height", DEFAULT_SIG_HEIGHT, 1);

    env->fill.sig.widget_name = env->fill.input_key;
    env->fill.sig.page_num = env->page_num;

    json_font = json_object_get(env->fill.json_map_item, "font");
    if(json_is_string(json_font)) {
        env->fill.sig.font = json_string_value(json_font);
        env->fill.sig.visible = 1;
    } else {
        env->fill.sig.font = 0;
        env->fill.sig.visible = 0;
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


fill_type map_input_textfield(pdf_env *env, fill_type success_type) {
    pos_data *pos = &env->fill.text.pos;
    json_t *json_pos = json_object_get(env->fill.json_map_item, "rect");

    pos->left = map_input_number(json_pos, "left", 0, 0);
    pos->top = map_input_number(json_pos, "top", 0, 0);
    pos->right = pos->left + map_input_number(json_pos, "width", DEFAULT_TEXT_WIDTH, 1);
    pos->bottom = pos->top + map_input_number(json_pos, "height", DEFAULT_TEXT_HEIGHT, 1);

    if(env->fill.text.pos.left < 0 || env->fill.text.pos.top < 0) {
        RETURN_FILL_ERROR("Position invalid");
    }

    json_t *json_edit = json_object_get(env->fill.json_map_item, "editable");
    if(json_is_boolean(json_edit)) {
        env->fill.text.editable = json_boolean_value(json_edit);
    } else {
        env->fill.text.editable = 0;
    }

    json_t *json_font = json_object_get(env->fill.json_map_item, "font");
    if(json_is_string(json_font)) {
        env->fill.text.font = json_string_value(json_font);
    } else {
        env->fill.text.font = 0;
    }

    json_t *json_color= json_object_get(env->fill.json_map_item, "color");
    if(json_is_array(json_color) && json_array_size(json_color) == 3) {
        env->fill.text.color[0] = json_number_value(json_array_get(json_color, 0));
        env->fill.text.color[1] = json_number_value(json_array_get(json_color, 1));
        env->fill.text.color[2] = json_number_value(json_array_get(json_color, 2));
    } else {
        env->fill.text.color[0] = 0;
        env->fill.text.color[1] = 0;
        env->fill.text.color[2] = 0;
    }

    return success_type;
}

fill_type map_input_image(pdf_env *env) {
    pos_data *pos = &env->fill.img.pos;
    json_t *json_pos;

    if((json_pos = json_object_get(env->fill.json_map_item, "rect")) != NULL) {
        pos->left = map_input_number(json_pos, "left", 0, 0);
        pos->top = map_input_number(json_pos, "top", 0, 0);
        pos->right = map_input_number(json_pos, "width", 0, 0);
        pos->bottom = map_input_number(json_pos, "height", 0, 0);
    } else if((json_pos = json_object_get(env->fill.json_map_item, "pos")) != NULL) {
        pos->left = map_input_number(json_pos, "left", 0, 0);
        pos->top = map_input_number(json_pos, "top", 0, 0);
        pos->right = 0;
        pos->bottom = 0;
    } else {
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

    if(strncmp(type_name, "textfield", 13) == 0) {
        return map_input_textfield(env, ADD_TEXTFIELD);
    } else if(strncmp(type_name, "signature", 9) == 0) {
        return map_input_signature(env);
    } else if(strncmp(type_name, "text", 5) == 0) {
        RETURN_FILL_ERROR("Text overlay not yet supported");
    } else if(strncmp(type_name, "image", 6) == 0) {
        return map_input_image(env);
    } else if(strncmp(type_name, "signature_logo", 6) == 0) {
        RETURN_FILL_ERROR("Logo signature not yet supported");
    } else {
        RETURN_FILL_ERROR("Trying to add an unrecognised type. Only 'textfield' and 'signature' supported.");
    }

}
