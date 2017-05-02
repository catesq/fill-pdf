#include "fill.h"

double map_input_number(json_t *jsn_obj, const char *property, float default_val, float min_val) {
    json_t *jsn_val = json_object_get(jsn_obj, property);

    if(json_is_number(jsn_val) && json_number_value(jsn_val) >= min_val) {
        return json_number_value(jsn_val);
    } else {
       return default_val;
    }
}

void map_input_posdata(json_t *jsn_obj, pos_data *pos, float default_xy, float default_width, float default_height) {
    pos->left = map_input_number(jsn_obj, "left", default_xy, 0);
    pos->top = map_input_number(jsn_obj, "top", default_xy, 0);
    pos->right = pos->left + map_input_number(jsn_obj, "width", default_width, 1);
    pos->bottom = pos->top + map_input_number(jsn_obj, "height", default_height, 1);
}


fill_type map_input_signature(pdf_env *env) {
    const char *sigfile = NULL;
    json_t *json_sigfile = NULL, *json_font = NULL, *json_pwd;
    struct stat buffer;

    map_input_posdata(json_object_get(env->fill.json_map_item, "rect"), &env->fill.sig.pos, 0, DEFAULT_SIG_WIDTH, DEFAULT_SIG_HEIGHT);

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

    return ADD_SIGNATURE;
}


fill_type map_input_textfield(pdf_env *env, fill_type success_type) {
    map_input_posdata(json_object_get(env->fill.json_map_item, "rect"), &env->fill.text.pos, -1, DEFAULT_TEXT_WIDTH, DEFAULT_TEXT_HEIGHT);

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
    } else {
        RETURN_FILL_ERROR("Trying to add an unrecognised type. Only 'textfield' and 'signature' supported.");
    }

}
